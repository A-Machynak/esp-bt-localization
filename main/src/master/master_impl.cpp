#include "master/master_impl.h"

#include "core/bt_common.h"
#include "core/device_data.h"
#include "core/gatt_common.h"
#include "core/utility/common.h"
#include "core/utility/uuid.h"
#include "master/http/api/post_data.h"
#include "master/nvs_utils.h"
#include "master/system_msg.h"

#include <esp_bt.h>
#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/FreeRTOSConfig.h>
#include <freertos/projdefs.h>
#include <freertos/task.h>
#include <freertos/timers.h>

#include <algorithm>
#include <cmath>

namespace
{
/// @brief Maximum block time in bluetooth callbacks
constexpr TickType_t BlockTimeInCallback = pdMS_TO_TICKS(25);

/// @brief Master application ID
constexpr std::uint16_t MasterAppId = 0;

/// @brief Logger tag
static const char * TAG = "Master";

static void UpdateScannersTask(void * pvParameters)
{
	reinterpret_cast<Master::Impl::App *>(pvParameters)->UpdateScannersLoop();
}

static void UpdateDeviceDataTask(void * pvParameters)
{
	reinterpret_cast<Master::Impl::App *>(pvParameters)->UpdateDeviceDataLoop();
}

}  // namespace

namespace Master::Impl
{
App::App(const AppConfig & cfg)
    : _cfg(cfg)
    , _bleGap(this)
{
}

void App::Init()
{
	// Reserve enough space, otherwise BTC will run out of it while allocating it himself for
	// some reason
	_tmpSerializedData.reserve(Core::DeviceDataView::Size * 128);
	_tmpScanners.reserve(10);

	// Mutex for DeviceMemory
	_memMutex = xSemaphoreCreateMutex();
	assert(_memMutex != nullptr);

	Bt::EnableBtController();
	Bt::EnableBluedroid();

	_bleGap.Init();
	_gattc.RegisterApp(MasterAppId, this);
	_gattc.SetLocalMtu(std::numeric_limits<std::uint16_t>::max());

	if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
		// Scheduler for timers
		vTaskStartScheduler();
	}

	constexpr auto StackSize1 = std::max(static_cast<std::uint32_t>(2 * 16'424),
	                                     static_cast<std::uint32_t>(configMINIMAL_STACK_SIZE));
	// Task for updating scanners
	auto ret = xTaskCreate(UpdateScannersTask, "Scanners loop", StackSize1, this, tskIDLE_PRIORITY,
	                       &_updateScannersTask);
	assert(ret == pdPASS);

	constexpr auto StackSize2 = std::max(static_cast<std::uint32_t>(2 * 16'424),
	                                     static_cast<std::uint32_t>(configMINIMAL_STACK_SIZE));
	// Task for reading scanner data
	ret = xTaskCreate(UpdateDeviceDataTask, "Read loop", StackSize2, this, tskIDLE_PRIORITY,
	                  &_readDevTask);
	assert(ret == pdPASS);

	// Scan for scanners
	_ScanForScanners();

	// Finally, initialize http server
	_httpServer.Init(_cfg.WifiCfg);
	_httpServer.SetConfigPostListener(
	    [&](std::span<const char> data) { OnHttpServerUpdate(data); });
}

void App::OnHttpServerUpdate(std::span<const char> data)
{
	// Update from http server
	const std::span span(reinterpret_cast<const std::uint8_t *>(data.data()), data.size());
	auto view = HttpApi::DevicesPostDataView(span);
	ESP_LOGI(TAG, "Config update");

	for (auto v = view.Next(); !std::holds_alternative<std::monostate>(v); v = view.Next()) {
		std::visit(
		    // Don't implement auto so we get a compile time error if an implementation is missing
		    Overload{
		        [&](const HttpApi::Type::SystemMsg & t) { ProcessSystemMessage(t.Value()); },
		        [&](const HttpApi::Type::RefPathLoss & t) {
			        Nvs::SetRefPathLoss(t.Mac(), t.Value());
		        },
		        [&](const HttpApi::Type::EnvFactor & t) { Nvs::SetEnvFactor(t.Mac(), t.Value()); },
		        [&](const HttpApi::Type::MacName & t) { Nvs::SetMacName(t.Mac(), t.Value()); },
		        [&](std::monostate t) {},
		    },
		    v);
	}
}

void App::GapBleScanResult(const Gap::Ble::Type::ScanResult & p)
{
	const Bt::Device device(p);

	if (xSemaphoreTake(_memMutex, BlockTimeInCallback)) {
		const bool IsConnectedScanner = _memory.IsConnectedScanner(device);  // Read
		xSemaphoreGive(_memMutex);

		if (!IsConnectedScanner && _IsScanner(device)) {
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
	else {
		ESP_LOGD(TAG, "Mtx take fail (GapScanRes)");
	}
	ESP_LOGD(TAG, "ScanRes: %s", ToString(device).c_str());
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
	ESP_LOGD(TAG,
	         "Update connection - { status: %d mac: %s minInt: %d maxInt: %d lat: %d cInt: %d "
	         "tout: %d }",
	         p.status, ToString(p.bda).c_str(), p.min_int, p.max_int, p.latency, p.conn_int,
	         p.timeout);
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
	ESP_LOGI(TAG, "Disconnect (%d)", p.reason);

	if (xSemaphoreTake(_memMutex, portMAX_DELAY)) {  // this HAS to be taken
		_memory.RemoveScanner(p.conn_id);            // Write
		xSemaphoreGive(_memMutex);
	}
	else {
		ESP_LOGW(TAG, "Mtx take fail (GattcDisconnect), unsynchronized write");

		// We might as well try to do an unsynchronized write. It might fail and restart,
		// but we don't care at that point.
		// TODO: Should fix this by trying to mark the scanner as unused in the memory and
		// deleting it from somewhere else.
		_memory.RemoveScanner(p.conn_id);
	}
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
	if (p.value_len == 0) {
		return;
	}

	// Responses from scanners
	if ((p.value_len % Core::DeviceDataView::Size) != 0) {
		if (xSemaphoreTake(_memMutex, BlockTimeInCallback)) {
			const auto * scanner = _memory.GetScanner(p.conn_id);  // Read
			xSemaphoreGive(_memMutex);

			ESP_LOGW(TAG, "Received incorrect data size from %s (%d %% %d != 0)",
			         scanner ? ToString(scanner->Info.Bda).c_str() : "UNKNOWN", p.value_len,
			         Core::DeviceDataView::Size);
		}
		else {
			ESP_LOGW(TAG, "Received incorrect data size from scanner conn id %d (%d %% %d != 0)",
			         p.conn_id, p.value_len, Core::DeviceDataView::Size);
		}
	}

	// Assume its an array of devices
	Core::DeviceDataView::Array data(std::span(p.value, p.value_len));

	if (xSemaphoreTake(_memMutex, BlockTimeInCallback)) {
		_memory.UpdateDistance(p.conn_id, data);  // Write
		xSemaphoreGive(_memMutex);
	}
	else {
		ESP_LOGD(TAG, "Mtx take fail (GattcRead)");
	}
}

void App::GattcSearchCmpl(const Gattc::Type::SearchCmpl & p)
{
	// Get all characteristics - 'State', 'Devices'
	auto sIt =
	    std::find_if(_tmpScanners.begin(), _tmpScanners.end(),
	                 [id = p.conn_id](const ScannerInfo & info) { return info.ConnId == id; });
	if (sIt == _tmpScanners.end()) {
		return;
	}

	// Get Characteristics from local cache
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
			         "Incorrect characteristic length (%d), incompatible Scanner. Disconnecting...",
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
	if (xSemaphoreTake(_memMutex, BlockTimeInCallback)) {
		_memory.AddScanner(*sIt);  // Write
		xSemaphoreGive(_memMutex);
	}
	else {
		ESP_LOGD(TAG, "Mtx take fail (GattcSearchCmpl)");
		_gattc.Disconnect(MasterAppId, p.conn_id);
	}
	_tmpScanners.erase(sIt);

	// Connected to scanner, resume scanning for others
	_ScanForScanners();
}

void App::GattcSearchRes(const Gattc::Type::SearchRes & p)
{
	// Search Characteristics result
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
	constexpr TickType_t Delay = pdMS_TO_TICKS(10'000);
	for (;;) {
		vTaskDelay(Delay);

		// Check if some scanners need to advertise
		if (!xSemaphoreTake(_memMutex, portMAX_DELAY)) {
			ESP_LOGD(TAG, "Mtx take fail (UpdateScannersLoop)");
			continue;
		}

		const auto advertiseInfo = _memory.GetScannerToAdvertise();  // Read
		if (advertiseInfo) {
			const auto connId = advertiseInfo->ConnId;
			const auto stateChar = advertiseInfo->Service.StateChar;
			xSemaphoreGive(_memMutex);

			// Force advertising state
			std::uint8_t val = Gatt::StateChar::Advertise;
			esp_ble_gattc_write_char(_gattcApp->GattIf, connId, stateChar, 1, &val,
			                         ESP_GATT_WRITE_TYPE_RSP, ESP_GATT_AUTH_REQ_NONE);
			ESP_LOGD(TAG, "Scanner %d should advertise", connId);
		}
		else {
			xSemaphoreGive(_memMutex);
		}
	}
}

void App::UpdateDeviceDataLoop()
{
	// Read "Devices" characteristic in regular intervals, to get updated RSSI values.
	// Each time all the scanners' values are read, update the new positions.

	constexpr TickType_t Delay = pdMS_TO_TICKS(5'500);
	constexpr TickType_t DelayBetweenReads = pdMS_TO_TICKS(500);

	// Data required to read a characteristic
	struct ReadCharData
	{
		std::uint16_t ConnectionId;
		std::uint16_t DeviceCharHandle;
	};

	static std::vector<ReadCharData> readCharData;
	readCharData.reserve(10);

	for (;;) {
		ESP_LOGI(TAG, "free %lu internal %lu min %lu", esp_get_free_heap_size(),
		         esp_get_free_internal_heap_size(), esp_get_minimum_free_heap_size());

		// First make a copy of each scanner's connection id and handle
		if (xSemaphoreTake(_memMutex, portMAX_DELAY)) {
			const auto & scanners = _memory.GetScanners();  // Read
			readCharData.resize(scanners.size());
			for (std::size_t i = 0; i < scanners.size(); i++) {
				readCharData[i].ConnectionId = scanners[i].Info.ConnId;
				readCharData[i].DeviceCharHandle = scanners[i].Info.Service.DevicesChar;
			}
		}
		xSemaphoreGive(_memMutex);

		// Now read data from each scanner; add a small delay between reads
		for (const auto & data : readCharData) {
			esp_ble_gattc_read_char(_gattcApp->GattIf, data.ConnectionId, data.DeviceCharHandle,
			                        ESP_GATT_AUTH_REQ_NONE);
			vTaskDelay(DelayBetweenReads);
		}
		vTaskDelay(Delay);

		// Update positions
		std::size_t size = 0;
		if (xSemaphoreTake(_memMutex, portMAX_DELAY)) {
			size = _memory.UpdateDevicePositions().size();  // Write
			xSemaphoreGive(_memMutex);
		}
		else {
			ESP_LOGD(TAG, "Mtx take fail (UpdateDeviceDataLoop[1])");
		}

		if (size != 0) {
			// Small delay in case it took too long
			vTaskDelay(DelayBetweenReads / 2);
			if (xSemaphoreTake(_memMutex, portMAX_DELAY)) {
				// Serialize
				const std::span<std::uint8_t> rawData = _memory.SerializeOutput();  // Read
				xSemaphoreGive(_memMutex);

				// and finally update the HTTP server data
				_httpServer.SetDevicesGetData(
				    std::span(reinterpret_cast<char *>(rawData.data()), rawData.size()));
			}
			else {
				ESP_LOGD(TAG, "Mtx take fail (UpdateDeviceDataLoop[2])");
			}
		}
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

	std::size_t connected = 4;
	if (xSemaphoreTake(_memMutex, portMAX_DELAY)) {
		connected = _memory.GetScanners().size();
		xSemaphoreGive(_memMutex);
	}
	if (connected >= 4 && InitState) {
		InitState = false;
		esp_ble_scan_params_t scanParams{
		    .scan_type = esp_ble_scan_type_t::BLE_SCAN_TYPE_PASSIVE,
		    .own_addr_type = esp_ble_addr_type_t::BLE_ADDR_TYPE_PUBLIC,
		    .scan_filter_policy = esp_ble_scan_filter_t::BLE_SCAN_FILTER_ALLOW_ALL,
		    .scan_interval = Gap::Ble::ConvertScanInterval(3.0),
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
		    .scan_interval = Gap::Ble::ConvertScanInterval(1.0),
		    .scan_window = Gap::Ble::ConvertScanInterval(0.3),
		    .scan_duplicate = esp_ble_scan_duplicate_t::BLE_SCAN_DUPLICATE_DISABLE,
		};
		_bleGap.SetScanParams(&scanParams);
	}
	_bleGap.StartScanning();
}

}  // namespace Master::Impl
