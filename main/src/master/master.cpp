#include "master/master.h"

#include "core/bt_common.h"
#include "core/device_data.h"
#include "core/gatt_common.h"
#include "core/utility/uuid.h"

#include <esp_bt.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/FreeRTOSConfig.h>
#include <freertos/projdefs.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <sdkconfig.h>

#include <algorithm>

namespace
{
/// @brief `Master` application ID
constexpr std::uint16_t MasterAppId = 0;

/// @brief Logger tag
static const char * TAG = "Master";

static void UpdateScannersTask(void * pvParameters)
{
	auto * master = reinterpret_cast<Master::App *>(pvParameters);
	master->UpdateScannersLoop();
}

static void UpdateDeviceDataTask(void * pvParameters)
{
	auto * master = reinterpret_cast<Master::App *>(pvParameters);
	master->UpdateDeviceDataLoop();
}

}  // namespace

namespace Master
{
App::App()
    : _bleGap(this)
{
}

void App::Init(const WifiConfig & config)
{
	Bt::EnableBtController();
	Bt::EnableBluedroid();

	_tmpSerializedData.reserve(Core::DeviceDataView::Size * 128);

	_bleGap.Init();
	_gattc.RegisterApp(MasterAppId, this);
	_gattc.SetLocalMtu(std::numeric_limits<std::uint16_t>::max());

	// Scheduler for timers
	if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
		// Task scheduler required for timers
		vTaskStartScheduler();
	}
	// Scan for scanners
	_ScanForScanners();

	// Task for updating scanners
	auto ret = xTaskCreate(UpdateScannersTask, "Scanners loop", configMINIMAL_STACK_SIZE * 2, this,
	                       tskIDLE_PRIORITY, &_updateScannersTask);
	assert(ret == pdPASS);

	// Task for reading scanner data
	ret = xTaskCreate(UpdateDeviceDataTask, "Read loop", configMINIMAL_STACK_SIZE * 2, this,
	                  tskIDLE_PRIORITY, &_readDevTask);
	assert(ret == pdPASS);

	// Finally, init http server
	_httpServer.Init(config);
}

void App::GapBleScanResult(const Gap::Ble::Type::ScanResult & p)
{
	/*if (_memory.GetData().size() >= ScannerConnectionLimit) {
	    ESP_LOGW(TAG, "Reached maximum number of scanners (%d). Possible bad actor?",
	             ScannerConnectionLimit);
	    return;
	}*/

	const Bt::Device device(p);
	const auto scannerIdx = _memory.GetConnectedScannerIdx(device);
	if ((scannerIdx == InvalidScannerIdx) && _IsScanner(device)) {
		// New scanner
		ESP_LOGI(TAG, "Found scanner (%s)", ToString(device.Bda).c_str());
		// We have to stop scanning while connecting, BUT we can't
		// connect before we actually stop scanning (it takes a while).
		// So we have to move it to GapBleScanStopCmpl().
		_scannerToConnect = device.Bda;
		_bleGap.StopScanning();
	}

	// ... otherwise it's some device and we don't care
}

void App::GapBleScanStopCmpl(const Gap::Ble::Type::ScanStopCmpl & p)
{
	// Stopped scanning. Now we can connect to a Scanner.
	if (_scannerToConnect.has_value()) {
		ESP_LOGI(TAG, "Connecting to %s", ToString(_scannerToConnect.value()).c_str());
		esp_ble_gattc_open(_gattcApp->GattIf, _scannerToConnect.value().Addr.data(),
		                   BLE_ADDR_TYPE_PUBLIC, true);
	}
	else {
		// We stopped scanning for some reason; we should always scan for Scanners.
		_ScanForScanners();
	}
	_scannerToConnect = std::nullopt;
}

void App::GapBleUpdateConn(const Gap::Ble::Type::UpdateConn & p)
{
	// ESP_LOGI(TAG, "st: %d mac: %s minInt: %d maxInt: %d lat: %d cInt: %d tout: %d", p.status,
	//          ToString(Bt::Address(p.bda)).c_str(), p.min_int, p.max_int, p.latency, p.conn_int,
	//          p.timeout);
}

void App::GattcReg(const Gattc::Type::Reg & p)
{
	_gattcApp = _gattc.GetAppInfo(MasterAppId);
	assert(_gattcApp != nullptr);
}

void App::GattcOpen(const Gattc::Type::Open & p)
{
	if (p.status != ESP_GATT_OK) {
		ESP_LOGE(TAG, "Unable to connect to device (%d)", p.status);
		_ScanForScanners();
		return;
	}
	// Set MTU
	esp_ble_gattc_send_mtu_req(_gattcApp->GattIf, p.conn_id);

	// Save; this needs to be done in several steps, since we have to read characteristic handles,
	// etc.
	_tmpScanners.emplace_back(p.conn_id, Mac(p.remote_bda));

	ESP_LOGI(TAG, "Connected to scanner, searching services...");
	esp_ble_gattc_search_service(_gattcApp->GattIf, p.conn_id, nullptr);

	esp_ble_conn_update_params_t params{
	    .bda = {p.remote_bda[0], p.remote_bda[1], p.remote_bda[2], p.remote_bda[3], p.remote_bda[4],
	            p.remote_bda[5]},
	    .min_int = ESP_BLE_CONN_INT_MIN,
	    .max_int = 200,
	    .latency = 0,
	    .timeout = 2000,  // N * 10ms
	};
	esp_ble_gap_update_conn_params(&params);
}

void App::GattcCancelOpen()
{
	// couldn't connect, restart scan
	_ScanForScanners();
}

void App::GattcDisconnect(const Gattc::Type::Disconnect & p)
{
	ESP_LOGI(TAG, "Disconnect - %d", p.reason);
	_memory.RemoveScanner(p.conn_id);
}

void App::GattcClose(const Gattc::Type::Close & p)
{
	ESP_LOGI(TAG, "Connection to %s terminated (%s)", ToString(Mac(p.remote_bda)).c_str(),
	         ToString(p.reason));

	auto it =
	    std::find_if(_tmpScanners.begin(), _tmpScanners.end(),
	                 [connId = p.conn_id](const ScannerInfo & cs) { return cs.ConnId == connId; });
	if (it != _tmpScanners.end()) {
		_tmpScanners.erase(it);
	}

	// Restart scan
	_ScanForScanners();
}

void App::GattcReadChar(const Gattc::Type::ReadChar & p)
{
	// Responses from scanners

	const std::size_t count = p.value_len / Core::DeviceDataView::Size;
	_tmpDevices.clear();
	_tmpDevicesRssi.clear();

	for (std::size_t i = 0; i < count; i++) {
		// Ptr offset
		const std::size_t offset = i * Core::DeviceDataView::Size;

		// Don't copy anything, just get a span and a view to the data
		const std::span<std::uint8_t, Core::DeviceDataView::Size> span(p.value + offset,
		                                                               Core::DeviceDataView::Size);
		const Core::DeviceDataView view(span);

		// ESP_LOGI(TAG, "Found %s - %d", ToString(Bt::Address(view.Mac())).c_str(),
		// (int)view.Rssi());
		_tmpDevices.emplace_back(view.Mac());
		_tmpDevicesRssi.push_back(view.Rssi());
	}
	_memory.UpdateDistance(p.conn_id, _tmpDevices, _tmpDevicesRssi);
	// ESP_LOGI(TAG, "Scanner read (%d) from %d", (int)count, p.conn_id);
}

void App::GattcSearchCmpl(const Gattc::Type::SearchCmpl & p)
{
	// Get all characteristics - 'State', 'Devices'
	auto sIt = _tmpScanners.begin();
	for (; sIt != _tmpScanners.end(); sIt++) {
		if (sIt->ConnId == p.conn_id)
			break;
	}
	if (sIt == _tmpScanners.end()) {
		return;
	}

	// Get Characteristics from local cahce
	std::uint16_t count = Gatt::ScannerServiceCharCount;
	esp_gattc_char_elem_t result[Gatt::ScannerServiceCharCount];
	esp_gatt_status_t err =
	    esp_ble_gattc_get_all_char(_gattcApp->GattIf, p.conn_id, sIt->Service.StartHandle,
	                               sIt->Service.EndHandle, result, &count, 0);

	if (err != ESP_GATT_OK) {
		ESP_LOGW(TAG, "Failed getting characteristics (%d). Disconnecting...", err);
		_gattc.Disconnect(MasterAppId, p.conn_id);
		return;
	}
	if (count < Gatt::ScannerServiceCharCount) {
		ESP_LOGI(TAG, "Missing characteristic, incompatible Scanner (%d != %d). Disconnecting...",
		         count, Gatt::ScannerServiceCharCount);
		_gattc.Disconnect(MasterAppId, p.conn_id);
		return;
	}

	// Validate and save Characteristics' handles
	for (std::uint16_t i = 0; i < count; i++) {
		if (result[i].uuid.len != 16) {
			ESP_LOGW(TAG,
			         "Incorrect characteristic length, incompatible Scanner (%d). Disconnecting...",
			         result[i].uuid.len);
			_gattc.Disconnect(MasterAppId, p.conn_id);
			return;
		}

		if (result[i].uuid == Gatt::StateCharacteristicArray) {
			sIt->Service.StateChar = result[i].char_handle;
		}
		else if (result[i].uuid == Gatt::DevicesCharacteristicArray) {
			sIt->Service.DevicesChar = result[i].char_handle;
		}
		else {
			ESP_LOGW(TAG, "Unknown characteristic, incompatible Scanner (%s). Disconnecting...",
			         ToString(result[i].uuid).c_str());
			_gattc.Disconnect(MasterAppId, p.conn_id);
			return;
		}
	}

	ESP_LOGI(TAG, "Saved characteristic handles");
	// Scanner info filled, add it and remove the temporary
	_memory.AddScanner(*sIt);
	_tmpScanners.erase(sIt);

	// Connected to scanner, resume scanning for others
	_ScanForScanners();
}

void App::GattcSearchRes(const Gattc::Type::SearchRes & p)
{
	ESP_LOGI(TAG, "SearchRes: start %d, end %d, inst_id %d, uuid %s, primary %d", p.start_handle,
	         p.end_handle, p.srvc_id.inst_id, ToString(p.srvc_id.uuid).c_str(), p.is_primary);
	constexpr std::array uuid = Util::UuidToArray(Gatt::ScannerService);

	if (p.srvc_id.uuid.len == 16
	    && std::equal(uuid.begin(), uuid.end(), p.srvc_id.uuid.uuid.uuid128)) {
		// Save Scanner service start/end handles
		for (auto & s : _tmpScanners) {  // find it first
			if (s.ConnId == p.conn_id) {
				s.Service.StartHandle = p.start_handle;
				s.Service.EndHandle = p.end_handle;
				break;
			}
		}
	}
}

void App::UpdateScannersLoop()
{
	// Check in regular intervals, if some scanner is missing an RSSI to another one.
	// If so, set its State characteristic to "Advertise".

	const std::vector<ScannerInfo> & scanners = _memory.GetScanners();
	constexpr TickType_t Delay = pdMS_TO_TICKS(10'000);
	for (;;) {
		vTaskDelay(Delay);

		// Check if some scanners need to advertise
		const auto toAdvertiseIdx = _memory.GetScannerIdxToAdvertise();
		if (toAdvertiseIdx == InvalidScannerIdx) {
			// No more scanners need to be updated
			continue;
		}
		ESP_LOGI(TAG, "%d should advertise", toAdvertiseIdx);

		const ScannerInfo & toAdvertise = scanners.at(toAdvertiseIdx);
		// Force advertising state
		std::uint8_t val = Gatt::StateChar::Advertise;
		esp_ble_gattc_write_char(_gattcApp->GattIf, toAdvertise.ConnId,
		                         toAdvertise.Service.StateChar, 1, &val, ESP_GATT_WRITE_TYPE_RSP,
		                         ESP_GATT_AUTH_REQ_NONE);
	}
}

void App::UpdateDeviceDataLoop()
{
	// Read "Devices" characteristic in regular intervals, to get updated RSSI values.
	// Each time all the scanners' values are read, update the new positions.

	constexpr TickType_t Delay = pdMS_TO_TICKS(5'500);
	constexpr TickType_t DelayBetweenReads = pdMS_TO_TICKS(500);

	const std::vector<ScannerInfo> & scanners = _memory.GetScanners();
	for (;;) {

		// Read data from each scanner
		for (std::size_t i = 0; i < scanners.size(); i++) {
			// This...might crash if a scanner disconnects
			const ScannerInfo & sc = scanners.at(i);
			esp_ble_gattc_read_char(_gattcApp->GattIf, sc.ConnId, sc.Service.DevicesChar,
			                        ESP_GATT_AUTH_REQ_NONE);
			vTaskDelay(DelayBetweenReads);
		}
		vTaskDelay(Delay);

		// Update positions
		auto & devData = _memory.UpdateDevicePositions();
		// Small delays in case it took too long
		vTaskDelay(DelayBetweenReads / 2);
		std::span<std::uint8_t> rawData = _memory.SerializeOutput();
		vTaskDelay(DelayBetweenReads / 2);

		// Finally update the HTTP server data
		_httpServer.SetDeviceData(
		    std::span(reinterpret_cast<char *>(rawData.data()), rawData.size()));
		vTaskDelay(DelayBetweenReads / 2);
	}
}

bool App::_IsScanner(const Bt::Device & device)
{
	// Only BLE
	if (!device.IsBle()) {
		return false;
	}

	const Bt::BleSpecific & ble = device.GetBle();

	// Only a single record
	if (ble.EirData.Records.size() != 1) {
		return false;
	}

	// 128b Service ID record
	const Bt::BleSpecific::Eir::Record & record = ble.EirData.Records.at(0);
	if (record.Type != esp_ble_adv_data_type::ESP_BLE_AD_TYPE_128SRV_CMPL
	    || record.Data.size() != Util::UuidByteCount) {
		return false;
	}

	// Equal to the scanner service uuid
	return std::equal(record.Data.begin(), record.Data.end(), Gatt::ScannerServiceArray.begin());
}

void App::_ScanForScanners()
{
	// SetScanParams has a bit of an overhead. Mitigate it by only
	// setting the params if necessary.
	static bool InitState = false;

	const std::size_t connected = _memory.GetScanners().size();
	if (connected >= 4 && InitState) {
		InitState = false;
		esp_ble_scan_params_t scanParams{
		    .scan_type = esp_ble_scan_type_t::BLE_SCAN_TYPE_PASSIVE,
		    .own_addr_type = esp_ble_addr_type_t::BLE_ADDR_TYPE_PUBLIC,
		    .scan_filter_policy = esp_ble_scan_filter_t::BLE_SCAN_FILTER_ALLOW_ALL,
		    .scan_interval = Gap::Ble::ConvertScanInterval(2.0),
		    .scan_window = Gap::Ble::ConvertScanInterval(0.25),
		    .scan_duplicate = esp_ble_scan_duplicate_t::BLE_SCAN_DUPLICATE_ENABLE,
		};
		_bleGap.SetScanParams(&scanParams);
	}
	else if (connected < 4 && !InitState) {
		InitState = true;
		esp_ble_scan_params_t scanParams{
		    .scan_type = esp_ble_scan_type_t::BLE_SCAN_TYPE_PASSIVE,
		    .own_addr_type = esp_ble_addr_type_t::BLE_ADDR_TYPE_PUBLIC,
		    .scan_filter_policy = esp_ble_scan_filter_t::BLE_SCAN_FILTER_ALLOW_ALL,
		    .scan_interval = Gap::Ble::ConvertScanInterval(3.0),
		    .scan_window = Gap::Ble::ConvertScanInterval(0.3),
		    .scan_duplicate = esp_ble_scan_duplicate_t::BLE_SCAN_DUPLICATE_DISABLE,
		};
		_bleGap.SetScanParams(&scanParams);
	}
	_bleGap.StartScanning();
}

}  // namespace Master
