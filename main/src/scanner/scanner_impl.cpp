#include "scanner/scanner_impl.h"

#include "core/bt_common.h"
#include "core/wrapper/advertisement_data.h"
#include "core/wrapper/device.h"

#include <esp_gatt_common_api.h>
#include <esp_log.h>

#include <sys/time.h>  // settimeofday

namespace
{

/// @brief Scanner application ID
constexpr std::uint16_t ScannerAppId = 0;

/// @brief Logger tag
static const char * TAG = "Scanner";

constexpr TickType_t BlockTimeInCallbacks = pdMS_TO_TICKS(500);

}  // namespace

namespace Scanner::Impl
{

App::App(const AppConfig & cfg)
    : _cfg(cfg)
    , _bleGap(this)
    , _btGap(this)
    , _memory(_cfg.DeviceMemoryCfg)
{
	_serializeVec.reserve(512);
}

void App::Init()
{
	Bt::EnableBtController();
	Bt::EnableBluedroid();

	// Mutex for DeviceMemory
	_memMutex = xSemaphoreCreateMutex();
	assert(_memMutex != nullptr);

	// BLE
	_bleGap.Init();
	esp_ble_scan_params_t scanParams{
	    .scan_type = esp_ble_scan_type_t::BLE_SCAN_TYPE_PASSIVE,
	    .own_addr_type = esp_ble_addr_type_t::BLE_ADDR_TYPE_PUBLIC,
	    .scan_filter_policy = esp_ble_scan_filter_t::BLE_SCAN_FILTER_ALLOW_ALL,
	    .scan_interval = Gap::Ble::ConvertScanInterval(1.0),
	    .scan_window = Gap::Ble::ConvertScanInterval(0.6),
	    .scan_duplicate = esp_ble_scan_duplicate_t::BLE_SCAN_DUPLICATE_DISABLE,
	};
	_bleGap.SetScanParams(&scanParams);
	std::vector<std::uint8_t> advData =
	    Ble::AdvertisementDataBuilder::Builder().SetCompleteUuid128(Gatt::ScannerService).Finish();
	_bleGap.SetRawAdvertisingData(advData);

	if (_cfg.Mode != ScanMode::BleOnly) {
		// Don't initialize Classic if BLE only
		_btGap.Init();
		_btGap.SetScanMode(Gap::Bt::ConnectionMode::ESP_BT_NON_CONNECTABLE,
		                   Gap::Bt::DiscoveryMode::ESP_BT_NON_DISCOVERABLE);
	}

	// GATTs
	_gatts.RegisterApp(ScannerAppId, this);
	_gatts.SetLocalMtu(std::numeric_limits<std::uint16_t>::max());

	_AdvertiseDefault();
}

void App::GapBleScanResult(const Gap::Ble::ScanResult & p)
{
	if (_state == Gatt::StateChar::Scan
	    && (p.search_evt == esp_gap_search_evt_t::ESP_GAP_SEARCH_INQ_CMPL_EVT
	        || p.search_evt == esp_gap_search_evt_t::ESP_GAP_SEARCH_DISC_CMPL_EVT
	        || p.search_evt == esp_gap_search_evt_t::ESP_GAP_SEARCH_DI_DISC_CMPL_EVT)) {
		// Restart scan
		_ScanForDevices();
		return;
	}
	// Scan result - Found a device
	const Bt::Device dev(p);
	ESP_LOGD(TAG, "%s", ToString(dev).c_str());

	if (xSemaphoreTake(_memMutex, BlockTimeInCallbacks)) {
		_memory.AddDevice(dev);
		xSemaphoreGive(_memMutex);
	}
}

void App::GapBleAdvStopCmpl(const Gap::Ble::Type::AdvStopCmpl & p)
{
	// Stopped advertising
	if (_connStatus == ConnectionStatus::Disconnected) {
		_AdvertiseDefault();
	}
	else {
		// Change to scanning state
		const std::uint8_t value = Gatt::StateChar::Scan;
		esp_ble_gatts_set_attr_value(_appInfo->GattHandles[Handle::State], 1, &value);
		_ChangeState(Gatt::StateChar::Scan);
	}
}

void App::GapBtDiscRes(const Gap::Bt::DiscRes & p)
{
	// Bluetooth discovery result
	if (_connStatus == ConnectionStatus::Disconnected) {
		return;
	}

	const Bt::Device dev(p);
	ESP_LOGD(TAG, "%s", ToString(dev).c_str());

	if (xSemaphoreTake(_memMutex, BlockTimeInCallbacks)) {
		_memory.AddDevice(dev);
		xSemaphoreGive(_memMutex);
	}
}

void App::GapBtDiscStateChanged(const Gap::Bt::DiscStateChanged & p)
{
	// Restart scan
	if (p.state == esp_bt_gap_discovery_state_t::ESP_BT_GAP_DISCOVERY_STOPPED
	    && _state == Gatt::StateChar::Scan) {
		_ScanForDevices();
	}
}

void App::GattsRegister(const Gatts::Type::Register & p)
{
	// GATTs register, create attribute table
	_appInfo = _gatts.GetAppInfo(p.app_id);
	_gatts.CreateAttributeTable(0, _attributeTable, {0});
}

void App::GattsConnect(const Gatts::Type::Connect & p)
{
	// GATTs connect - connected to Master
	if (_connStatus == ConnectionStatus::Connected) {
		// Only a single connection is allowed
		_gatts.Close(_appInfo->AppId, p.conn_id);
		_AdvertiseDefault();
	}
	else {
		_connStatus = ConnectionStatus::Connected;
		_bleGap.StopAdvertising();
	}
}

void App::GattsDisconnect(const Gatts::Type::Disconnect & p)
{
	ESP_LOGI(TAG, "Disconnected");
	// Disconnected, restart advertising
	_connStatus = ConnectionStatus::Disconnected;
	_state = Gatt::StateChar::Advertise;
	_bleGap.StopScanning();
	_btGap.StopDiscovery();
	_AdvertiseDefault();
}

void App::GattsRead(const Gatts::Type::Read & p)
{
	if (p.handle != _appInfo->GattHandles[Handle::Devices]) {
		return;
	}
	// Manually respond to Devices GATT READ - reading is destructive.

	// Attributes are limited to 512 octets.
	constexpr std::size_t SizeLimit =
	    std::min(512u, ((ESP_GATT_MAX_MTU_SIZE - 1) / Core::DeviceDataView::Size)
	                       * Core::DeviceDataView::Size);
	esp_gatt_rsp_t rsp{};
	rsp.attr_value.handle = p.handle;
	rsp.attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
	rsp.attr_value.offset = p.offset;

	if (p.offset != 0) {
		// Don't update; send the rest of the data
		if (p.offset < _serializeVec.size()) {
			const std::size_t bToSend = SizeLimit - p.offset;
			const std::size_t size = std::min(bToSend, _serializeVec.size() - bToSend);
			std::copy(_serializeVec.begin() + p.offset, _serializeVec.begin() + size,
			          rsp.attr_value.value);
			rsp.attr_value.len = size;
		}  // else no data
		esp_ble_gatts_send_response(_appInfo->GattIf, p.conn_id, p.trans_id, ESP_GATT_OK, &rsp);
		return;
	}

	// Update
	_CheckAndUpdateDevicesData();

	if ((_serializeVec.size() == 0) && p.need_rsp) {
		// No data after update
		esp_ble_gatts_send_response(_appInfo->GattIf, p.conn_id, p.trans_id, ESP_GATT_OK, &rsp);
		return;
	}

	// Build a response
	const std::size_t size = std::min(SizeLimit, _serializeVec.size());
	std::copy_n(_serializeVec.data(), size, rsp.attr_value.value);
	rsp.attr_value.len = size;
	esp_ble_gatts_send_response(_appInfo->GattIf, p.conn_id, p.trans_id, ESP_GATT_OK, &rsp);
}

void App::GattsWrite(const Gatts::Type::Write & p)
{
	if (p.handle == _appInfo->GattHandles[Handle::State]) {
		// No idea why we can't use:
		// _attributeTable.Attributes[Handle::State]->at(0);
		// ... apparently the data isn't written there; where is it?
		std::uint16_t length = 0;
		const std::uint8_t * value;
		esp_ble_gatts_get_attr_value(p.handle, &length, &value);

		const auto state = static_cast<Gatt::StateChar>(*value);
		_ChangeState(state);
	}
	else if (p.handle == _appInfo->GattHandles[Handle::Timestamp]) {
		if (p.len != 4) {
			ESP_LOGI(TAG, "Invalid timestamp length (%d)", p.len);
			return;
		}
		const std::uint32_t value = *reinterpret_cast<std::uint32_t *>(p.value);
		ESP_LOGI(TAG, "Setting time to %lu", value);

		timeval tm{};
		tm.tv_sec = value;
		settimeofday(&tm, nullptr);
	}
}

void App::_ChangeState(const Gatt::StateChar state)
{
	_state = state;

	switch (state) {
	case Gatt::StateChar::Idle:
		_bleGap.StopAdvertising();
		_bleGap.StopScanning();
		_btGap.StopDiscovery();
		break;
	case Gatt::StateChar::Advertise:
		_bleGap.StopScanning();
		_btGap.StopDiscovery();
		_AdvertiseToBeacons();
		break;
	case Gatt::StateChar::Scan:
		_bleGap.StopAdvertising();
		_ScanForDevices();
		break;
	}
}

void App::_AdvertiseDefault()
{
	esp_ble_adv_params_t advParams{
	    .adv_int_min = Gap::Ble::ConvertAdvertisingInterval(0.3),
	    .adv_int_max = Gap::Ble::ConvertAdvertisingInterval(0.75),
	    .adv_type = esp_ble_adv_type_t::ADV_TYPE_IND,
	    .own_addr_type = esp_ble_addr_type_t::BLE_ADDR_TYPE_PUBLIC,
	    .peer_addr = {0},
	    .peer_addr_type = esp_ble_addr_type_t::BLE_ADDR_TYPE_PUBLIC,
	    .channel_map = esp_ble_adv_channel_t::ADV_CHNL_37,
	    .adv_filter_policy = esp_ble_adv_filter_t::ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
	};
	_bleGap.StartAdvertising(&advParams);
	ESP_LOGI(TAG, "Advertising to devices");
}

void App::_AdvertiseToBeacons()
{
	constexpr float AdvertisingLength = 7.5;
	esp_ble_adv_params_t advParams{
	    .adv_int_min = Gap::Ble::ConvertAdvertisingInterval(0.3),
	    .adv_int_max = Gap::Ble::ConvertAdvertisingInterval(0.5),
	    .adv_type = esp_ble_adv_type_t::ADV_TYPE_IND,
	    .own_addr_type = esp_ble_addr_type_t::BLE_ADDR_TYPE_PUBLIC,
	    .peer_addr = {0},
	    .peer_addr_type = esp_ble_addr_type_t::BLE_ADDR_TYPE_PUBLIC,
	    .channel_map = esp_ble_adv_channel_t::ADV_CHNL_37,
	    .adv_filter_policy = esp_ble_adv_filter_t::ADV_FILTER_ALLOW_SCAN_WLST_CON_WLST,
	};
	_bleGap.StartAdvertising(&advParams, AdvertisingLength);
	ESP_LOGI(TAG, "Advertising to beacons");
}

void App::_ScanForDevices()
{
	// Scan forever
	if (_cfg.Mode == ScanMode::Both) {
		static bool swap = false;
		if (!swap) {
			const float time = static_cast<float>(_cfg.ScanModePeriodBle);
			_bleGap.StartScanning(time);
			ESP_LOGI(TAG, "Scanning for devices (BLE)");
		}
		else {
			const float time = static_cast<float>(_cfg.ScanModePeriodClassic);
			_btGap.StartDiscovery(Gap::Bt::InquiryMode::ESP_BT_INQ_MODE_GENERAL_INQUIRY, time);
			ESP_LOGI(TAG, "Scanning for devices (Classic)");
		}
		swap = !swap;
	}
	else if (_cfg.Mode == ScanMode::ClassicOnly) {
		_btGap.StartDiscovery(Gap::Bt::InquiryMode::ESP_BT_INQ_MODE_GENERAL_INQUIRY,
		                      Gap::Bt::DiscoverForever);
		ESP_LOGI(TAG, "Scanning for devices (Classic)");
	}
	else {
		_bleGap.StartScanning(Gap::Ble::ScanForever);
		ESP_LOGI(TAG, "Scanning for devices (BLE)");
	}
}

void App::_CheckAndUpdateDevicesData()
{
	const auto now = Core::Clock::now();
	const auto diff = Core::DeltaMs(_lastDevicesUpdate, now);
	if (diff > _cfg.DevicesUpdateInterval) {
		_lastDevicesUpdate = now;
		_UpdateDevicesData();
	}
}

void App::_UpdateDevicesData()
{
	if (xSemaphoreTake(_memMutex, BlockTimeInCallbacks)) {
		_memory.SerializeData(_serializeVec);
		xSemaphoreGive(_memMutex);
	}
}
}  // namespace Scanner::Impl
