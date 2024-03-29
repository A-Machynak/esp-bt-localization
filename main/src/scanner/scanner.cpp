#include "scanner/scanner.h"

#include "core/bt_common.h"
#include "core/wrapper/advertisement_data.h"

#include "core/wrapper/device.h"

#include <esp_log.h>

namespace
{

/// @brief `Scanner` application ID
constexpr std::uint16_t ScannerAppId = 0;

/// @brief Logger tag
static const char * TAG = "Scanner";

}  // namespace

namespace Scanner
{

App::App()
    : _bleGap(this)
    , _btGap(this)
{
	_serializeVec.reserve(DeviceMemoryByteSize());
}

void App::Init()
{
	Bt::EnableBtController();
	Bt::EnableBluedroid();

	_bleGap.Init();
	_btGap.Init();
	_gatts.RegisterApp(ScannerAppId, this);

	esp_ble_scan_params_t scanParams{
	    .scan_type = esp_ble_scan_type_t::BLE_SCAN_TYPE_PASSIVE,
	    .own_addr_type = esp_ble_addr_type_t::BLE_ADDR_TYPE_PUBLIC,
	    .scan_filter_policy = esp_ble_scan_filter_t::BLE_SCAN_FILTER_ALLOW_ALL,
	    .scan_interval = Gap::Ble::ConvertScanInterval(1.0),
	    .scan_window = Gap::Ble::ConvertScanInterval(0.6),
	    .scan_duplicate = esp_ble_scan_duplicate_t::BLE_SCAN_DUPLICATE_DISABLE,
	};
	_bleGap.SetScanParams(&scanParams);

	_btGap.SetScanMode(Gap::Bt::ConnectionMode::ESP_BT_NON_CONNECTABLE,
	                   Gap::Bt::DiscoveryMode::ESP_BT_NON_DISCOVERABLE);
	_gatts.SetLocalMtu(std::numeric_limits<std::uint16_t>::max());

	std::vector<std::uint8_t> advData =
	    Ble::AdvertisementDataBuilder::Builder().SetCompleteUuid128(Gatt::ScannerService).Finish();
	_bleGap.SetRawAdvertisingData(advData);

	_AdvertiseDefault();
}

void App::GapBleScanResult(const Gap::Ble::ScanResult & p)
{
	// Scan result - Found a device
	const Bt::Device dev(p);
	ESP_LOGI(TAG, "%s", ToString(dev).c_str());
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
	ESP_LOGI(TAG, "%s", ToString(dev).c_str());

	_CheckAndUpdateDevicesData();
}

void App::GattsRegister(const Gatts::Type::Register & p)
{
	// GATTs register, create attribute table
	_appInfo = _gatts.GetAppInfo(p.app_id);
	std::vector<std::size_t> v{0};
	_gatts.CreateAttributeTable(0, _attributeTable, v);
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
	_connStatus = ConnectionStatus::Disconnected;
	_bleGap.StopScanning();
	_btGap.StopDiscovery();
	_AdvertiseDefault();
}

void App::GattsRead(const Gatts::Type::Read & p)
{
	//ESP_LOGI(TAG, "Read");
	_CheckAndUpdateDevicesData();
}

void App::GattsWrite(const Gatts::Type::Write & p)
{
	ESP_LOGI(TAG, "Write %s", ToString(p).c_str());

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

void App::GattsExecWrite(const Gatts::Type::ExecWrite &)
{
	ESP_LOGI(TAG, "Exec write");
}

void App::_ChangeState(const Gatt::StateChar state)
{
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
	    .adv_int_max = Gap::Ble::ConvertAdvertisingInterval(1.0),
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
	constexpr float AdvertisingLength = 5.0;
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
	// scan forever
	_bleGap.StartScanning(Gap::Ble::ScanForever);
	_btGap.StartDiscovery(Gap::Bt::InquiryMode::ESP_BT_INQ_MODE_GENERAL_INQUIRY,
	                      Gap::Bt::DiscoverForever);
	ESP_LOGI(TAG, "Scanning for devices");
}

void App::_CheckAndUpdateDevicesData()
{
	constexpr std::int64_t MsToUpdate = 5'000;
	const auto now = Clock::now();
	const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - _lastDevicesUpdate).count();
	if (diff > MsToUpdate) {
		_lastDevicesUpdate = now;
		_UpdateDevicesData();
	}
}

void App::_UpdateDevicesData()
{
	_memory.SerializeData(_serializeVec);
	if (_serializeVec.size() > 0) {
		esp_ble_gatts_set_attr_value(_appInfo->GattHandles[Handle::Devices], _serializeVec.size(),
		                             _serializeVec.data());
	}
}
}  // namespace Scanner
