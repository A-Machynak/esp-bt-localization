#include "scanner/scanner_impl.h"

#include "core/bt_common.h"
#include "core/wrapper/advertisement_data.h"
#include "core/wrapper/device.h"

#include <esp_gatt_common_api.h>
#include <esp_log.h>

namespace
{

/// @brief Scanner application ID
constexpr std::uint16_t ScannerAppId = 0;

/// @brief Logger tag
static const char * TAG = "Scanner";

}  // namespace

namespace Scanner::Impl
{

App::App()
    : _bleGap(this)
    , _btGap(this)
{
}

void App::Init(const AppConfig & cfg)
{
	_cfg = cfg;

	Bt::EnableBtController();
	Bt::EnableBluedroid();

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

	if (cfg.Mode != ScanMode::BleOnly) {
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
	_memory.AddDevice(dev);

	_CheckAndUpdateDevicesData();
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

	Bt::Device dev(p);
	ESP_LOGD(TAG, "%s", ToString(dev).c_str());

	_CheckAndUpdateDevicesData();
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
		esp_ble_gatts_close(_appInfo->GattIf, p.conn_id);
		_AdvertiseDefault();
	}
	else {
		_connStatus = ConnectionStatus::Connected;
		_bleGap.StopAdvertising();
	}
}

void App::GattsDisconnect(const Gatts::Type::Disconnect & p)
{
	// Disconnected, restart advertising
	_state = Gatt::StateChar::Advertise;

	_connStatus = ConnectionStatus::Disconnected;
	_bleGap.StopScanning();
	_btGap.StopDiscovery();
	_AdvertiseDefault();
}

void App::GattsRead(const Gatts::Type::Read & p)
{
	// Attributes are limited to 512 octets. That would limit us to 7 devices.
	// To get around that, we can just keep shifting an offset and send some other
	// data for each read request.
	static std::size_t movingOffset = 0;
	constexpr std::size_t SizeLimit =
	    std::min(512u, ((ESP_GATT_MAX_MTU_SIZE - 1) / Core::DeviceDataView::Size)
	                       * Core::DeviceDataView::Size);

	ESP_LOGD(TAG, "Read %d %d %d", p.is_long, p.need_rsp, p.offset);
	_CheckAndUpdateDevicesData();

	const std::size_t totalOffset = movingOffset + p.offset;
	const std::size_t size = std::min(SizeLimit, _serializeVec.size() - totalOffset);
	if (totalOffset > _serializeVec.size() || size > _serializeVec.size()) {
		if (p.need_rsp) {
			esp_ble_gatts_send_response(_appInfo->GattIf, p.conn_id, p.trans_id, ESP_GATT_OK,
			                            nullptr);
		}
		return;
	}

	// If there is more data and this is not the last "block", indicate it with "ESP_GATT_MORE"
	const esp_gatt_status_t status =
	    ((_serializeVec.size() > SizeLimit) && ((movingOffset + SizeLimit) < _serializeVec.size()))
	        ? ESP_GATT_MORE
	        : ESP_GATT_OK;

	// Build a response
	esp_gatt_rsp_t rsp;
	rsp.attr_value.handle = p.handle;
	rsp.attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
	std::copy_n(_serializeVec.data() + totalOffset, size, rsp.attr_value.value);
	rsp.attr_value.len = size;
	rsp.attr_value.offset = p.offset;
	esp_ble_gatts_send_response(_appInfo->GattIf, p.conn_id, p.trans_id, status, &rsp);

	if (p.offset == 0) {
		// Move our custom offset.
		if ((movingOffset + SizeLimit) > _serializeVec.size()) {
			movingOffset = 0;
		}
		else {
			// Only shift enough to still be full
			movingOffset = std::min(movingOffset + SizeLimit, _serializeVec.size() - SizeLimit);
		}
	}
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
	    .channel_map = esp_ble_adv_channel_t::ADV_CHNL_ALL,
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
	    .channel_map = esp_ble_adv_channel_t::ADV_CHNL_ALL,
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
		const float time = static_cast<float>(_cfg.ScanModePeriod);
		if (!swap) {
			_bleGap.StartScanning(time);
			ESP_LOGI(TAG, "Scanning for devices (BLE)");
		}
		else {
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
	constexpr std::int64_t MsToUpdate = 5'000;
	const auto now = Clock::now();
	const auto diff =
	    std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastDevicesUpdate).count();
	if (diff > MsToUpdate) {
		_lastDevicesUpdate = now;
		_UpdateDevicesData();
	}
}

void App::_UpdateDevicesData()
{
	_memory.SerializeData(_serializeVec);
	// if (_serializeVec.size() > 0) {
	//  Not even going to set the attribute value. Pointless, since we are responding to
	//  reads/writes manually.
	//  These methods also do so many copies that it makes my head hurt.

	// esp_ble_gatts_set_attr_value(_appInfo->GattHandles[Handle::Devices],
	// _serializeVec.size(), _serializeVec.data());
	//}
}
}  // namespace Scanner::Impl
