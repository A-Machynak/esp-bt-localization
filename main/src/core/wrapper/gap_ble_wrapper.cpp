
#include "core/wrapper/gap_ble_wrapper.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/FreeRTOSConfig.h>
#include <freertos/projdefs.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <sdkconfig.h>

#include <esp_log.h>

namespace
{
/// @brief Logger tag
static const char * TAG = "GAP_BLE";

/// @brief There is nothing we can do...
static Gap::Ble::Wrapper * _Wrapper;

static void GapCallbackPassthrough(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t * param)
{
	ESP_LOGV(TAG, "%s", ToString(event));
	_Wrapper->BleGapCallback(event, param);
}

static void TimerCallback(TimerHandle_t xTimer)
{
	configASSERT(xTimer);

	auto * wrapper = reinterpret_cast<Gap::Ble::Wrapper *>(pvTimerGetTimerID(xTimer));
	wrapper->StopAdvertising();
}
}  // namespace

namespace Gap::Ble
{
Wrapper::Wrapper(IGapCallback * callback)
    : _callback(callback)
{
	_Wrapper = this;
}

void Wrapper::Init()
{
	if (xTaskGetSchedulerState() != taskSCHEDULER_RUNNING) {
		// Task scheduler required for timers
		vTaskStartScheduler();
	}
	// Create timer
	_advTimerHandle = xTimerCreateStatic("Advertising Timer", pdMS_TO_TICKS(5'000), pdFALSE, this,
	                                     TimerCallback, &_advTimerBuffer);
	if (_advTimerHandle == nullptr) {
		ESP_LOGE(TAG, "Failed creating advertising timer. This should never fail(?!)");
	}

	esp_ble_gap_register_callback(GapCallbackPassthrough);
}

Wrapper::~Wrapper() {}

void Wrapper::SetRawAdvertisingData(std::span<std::uint8_t> data)
{
	const std::size_t size = std::min(data.size(), (std::size_t)ESP_BLE_ADV_DATA_LEN_MAX);

	esp_err_t err = esp_ble_gap_config_adv_data_raw(data.data(), size);
	if (err != ESP_OK) {
		ESP_LOGW(TAG, "Failed setting advertising data (%d)", err);
	}
	else {
		ESP_LOGI(TAG, "Advertising data set");
	}
}

void Wrapper::StartAdvertising(esp_ble_adv_params_t * params, float time)
{
	if (_isAdvertising) {
		return;
	}
	ESP_ERROR_CHECK(esp_ble_gap_start_advertising(params));

	if (time > 0.0f) {  // don't advertise forever
		const float tfMs = time * 1000.0;
		const std::uint32_t tMs = (tfMs < static_cast<float>(portMAX_DELAY))
		                              ? static_cast<std::uint32_t>(tfMs)
		                              : portMAX_DELAY;

		// Don't wait for too long, if we are called from some callback.
		// Wouldn't it be easier to create a new timer instead?
		xTimerChangePeriod(_advTimerHandle, pdMS_TO_TICKS(tMs), 100);
		if (xTimerStart(_advTimerHandle, 100) != pdPASS) {
			ESP_LOGE(TAG, "Failed starting advertising timer");
		}
	}
	ESP_LOGI(TAG, "Advertising started");
	_isAdvertising = true;
}

void Wrapper::StopAdvertising()
{
	if (!_isAdvertising) {
		return;
	}
	esp_ble_gap_stop_advertising();
	ESP_LOGI(TAG, "Advertising stopped");
	_isAdvertising = false;
}

void Wrapper::SetScanParams(esp_ble_scan_params_t * params)
{
	esp_ble_gap_set_scan_params(params);
}

void Wrapper::StartScanning(float time)
{
	if (_isScanning) {
		ESP_LOGI(TAG, "Already scanning");
		return;
	}

	_scanForever = (time == ScanForever);
	time = std::clamp(time, 0.f, (float)std::numeric_limits<std::uint32_t>::max());
	ESP_ERROR_CHECK(esp_ble_gap_start_scanning(static_cast<std::uint32_t>(time)));
	ESP_LOGI(TAG, "Scanning started");
	_isScanning = true;
}

void Wrapper::StopScanning()
{
	if (!_isScanning) {
		ESP_LOGI(TAG, "Already not scanning");
		return;
	}
	_scanForever = false;
	_firstScanMessage = true;
	_isScanning = false;
	ESP_ERROR_CHECK(esp_ble_gap_stop_scanning());
	ESP_LOGI(TAG, "Scanning stopped");
}

void Wrapper::BleGapCallback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t * param)
{
	if (!_callback) {
		return;
	}

	switch (event) {
	case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
		_callback->GapBleAdvDataCmpl(param->adv_data_cmpl);
		break;
	case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
		_callback->GapBleScanRspDataCmpl(param->scan_rsp_data_cmpl);
		break;
	case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
		_callback->GapBleScanParamCmpl(param->scan_param_cmpl);
		break;
	case ESP_GAP_BLE_SCAN_RESULT_EVT: {
		const auto e = param->scan_rst.search_evt;
		if (e == esp_gap_search_evt_t::ESP_GAP_SEARCH_INQ_CMPL_EVT
		    || e == esp_gap_search_evt_t::ESP_GAP_SEARCH_DISC_CMPL_EVT
		    || e == esp_gap_search_evt_t::ESP_GAP_SEARCH_DI_DISC_CMPL_EVT) {
			_isScanning = false;  // ??? Why not SCAN_STOP event?
		}
		_callback->GapBleScanResult(param->scan_rst);
		break;
	}
	case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
		_callback->GapBleAdvDataRawCmpl(param->adv_data_raw_cmpl);
		break;
	case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT:
		_callback->GapBleScanRspDataRawCmpl(param->scan_rsp_data_raw_cmpl);
		break;
	case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
		_callback->GapBleAdvStartCmpl(param->adv_start_cmpl);
		break;
	case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
		_isScanning = true;
		if (_scanForever && _firstScanMessage) {  // send only the first callback
			_firstScanMessage = false;
			_callback->GapBleScanStartCmpl(param->scan_start_cmpl);
		}
		break;
	case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
		_callback->GapBleAdvStopCmpl(param->adv_stop_cmpl);
		break;
	case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
		_isScanning = false;
		if (_scanForever) {
			StartScanning(ScanForever);
		}
		else {  // sends only the last callback if scanning forever
			_callback->GapBleScanStopCmpl(param->scan_stop_cmpl);
		}
		break;
	case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
		_callback->GapBleUpdateConn(param->update_conn_params);
		break;
	default:
		break;
	}
}

}  // namespace Gap::Ble

const char * ToString(const esp_gap_ble_cb_event_t event)
{
	// clang-format off
	switch (event)
	{
	case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT: return "ADV_DATA_SET_COMPLETE_EVT";
	case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT: return "SCAN_RSP_DATA_SET_COMPLETE_EVT";
	case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: return "SCAN_PARAM_SET_COMPLETE_EVT";
	case ESP_GAP_BLE_SCAN_RESULT_EVT: return "SCAN_RESULT_EVT";
	case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT: return "ADV_DATA_RAW_SET_COMPLETE_EVT";
	case ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT: return "SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT";
	case ESP_GAP_BLE_ADV_START_COMPLETE_EVT: return "ADV_START_COMPLETE_EVT";
	case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT: return "SCAN_START_COMPLETE_EVT";
	case ESP_GAP_BLE_AUTH_CMPL_EVT: return "AUTH_CMPL_EVT";
	case ESP_GAP_BLE_KEY_EVT: return "KEY_EVT";
	case ESP_GAP_BLE_SEC_REQ_EVT: return "SEC_REQ_EVT";
	case ESP_GAP_BLE_PASSKEY_NOTIF_EVT: return "PASSKEY_NOTIF_EVT";
	case ESP_GAP_BLE_PASSKEY_REQ_EVT: return "PASSKEY_REQ_EVT";
	case ESP_GAP_BLE_OOB_REQ_EVT: return "OOB_REQ_EVT";
	case ESP_GAP_BLE_LOCAL_IR_EVT: return "LOCAL_IR_EVT";
	case ESP_GAP_BLE_LOCAL_ER_EVT: return "LOCAL_ER_EVT";
	case ESP_GAP_BLE_NC_REQ_EVT: return "NC_REQ_EVT";
	case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT: return "ADV_STOP_COMPLETE_EVT";
	case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT: return "SCAN_STOP_COMPLETE_EVT";
	case ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT: return "SET_STATIC_RAND_ADDR_EVT";
	case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT: return "UPDATE_CONN_PARAMS_EVT";
	case ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT: return "SET_PKT_LENGTH_COMPLETE_EVT";
	case ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT: return "SET_LOCAL_PRIVACY_COMPLETE_EVT";
	case ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT: return "REMOVE_BOND_DEV_COMPLETE_EVT";
	case ESP_GAP_BLE_CLEAR_BOND_DEV_COMPLETE_EVT: return "CLEAR_BOND_DEV_COMPLETE_EVT";
	case ESP_GAP_BLE_GET_BOND_DEV_COMPLETE_EVT: return "GET_BOND_DEV_COMPLETE_EVT";
	case ESP_GAP_BLE_READ_RSSI_COMPLETE_EVT: return "READ_RSSI_COMPLETE_EVT";
	case ESP_GAP_BLE_UPDATE_WHITELIST_COMPLETE_EVT: return "UPDATE_WHITELIST_COMPLETE_EVT";
	case ESP_GAP_BLE_UPDATE_DUPLICATE_EXCEPTIONAL_LIST_COMPLETE_EVT: return "UPDATE_DUPLICATE_EXCEPTIONAL_LIST_COMPLETE_EVT";
	case ESP_GAP_BLE_SET_CHANNELS_EVT: return "SET_CHANNELS_EVT";
	case ESP_GAP_BLE_READ_PHY_COMPLETE_EVT: return "READ_PHY_COMPLETE_EVT";
	case ESP_GAP_BLE_SET_PREFERRED_DEFAULT_PHY_COMPLETE_EVT: return "SET_PREFERRED_DEFAULT_PHY_COMPLETE_EVT";
	case ESP_GAP_BLE_SET_PREFERRED_PHY_COMPLETE_EVT: return "SET_PREFERRED_PHY_COMPLETE_EVT";
	case ESP_GAP_BLE_EXT_ADV_SET_RAND_ADDR_COMPLETE_EVT: return "EXT_ADV_SET_RAND_ADDR_COMPLETE_EVT";
	case ESP_GAP_BLE_EXT_ADV_SET_PARAMS_COMPLETE_EVT: return "EXT_ADV_SET_PARAMS_COMPLETE_EVT";
	case ESP_GAP_BLE_EXT_ADV_DATA_SET_COMPLETE_EVT: return "EXT_ADV_DATA_SET_COMPLETE_EVT";
	case ESP_GAP_BLE_EXT_SCAN_RSP_DATA_SET_COMPLETE_EVT: return "EXT_SCAN_RSP_DATA_SET_COMPLETE_EVT";
	case ESP_GAP_BLE_EXT_ADV_START_COMPLETE_EVT: return "EXT_ADV_START_COMPLETE_EVT";
	case ESP_GAP_BLE_EXT_ADV_STOP_COMPLETE_EVT: return "EXT_ADV_STOP_COMPLETE_EVT";
	case ESP_GAP_BLE_EXT_ADV_SET_REMOVE_COMPLETE_EVT: return "EXT_ADV_SET_REMOVE_COMPLETE_EVT";
	case ESP_GAP_BLE_EXT_ADV_SET_CLEAR_COMPLETE_EVT: return "EXT_ADV_SET_CLEAR_COMPLETE_EVT";
	case ESP_GAP_BLE_PERIODIC_ADV_SET_PARAMS_COMPLETE_EVT: return "PERIODIC_ADV_SET_PARAMS_COMPLETE_EVT";
	case ESP_GAP_BLE_PERIODIC_ADV_DATA_SET_COMPLETE_EVT: return "PERIODIC_ADV_DATA_SET_COMPLETE_EVT";
	case ESP_GAP_BLE_PERIODIC_ADV_START_COMPLETE_EVT: return "PERIODIC_ADV_START_COMPLETE_EVT";
	case ESP_GAP_BLE_PERIODIC_ADV_STOP_COMPLETE_EVT: return "PERIODIC_ADV_STOP_COMPLETE_EVT";
	case ESP_GAP_BLE_PERIODIC_ADV_CREATE_SYNC_COMPLETE_EVT: return "PERIODIC_ADV_CREATE_SYNC_COMPLETE_EVT";
	case ESP_GAP_BLE_PERIODIC_ADV_SYNC_CANCEL_COMPLETE_EVT: return "PERIODIC_ADV_SYNC_CANCEL_COMPLETE_EVT";
	case ESP_GAP_BLE_PERIODIC_ADV_SYNC_TERMINATE_COMPLETE_EVT: return "PERIODIC_ADV_SYNC_TERMINATE_COMPLETE_EVT";
	case ESP_GAP_BLE_PERIODIC_ADV_ADD_DEV_COMPLETE_EVT: return "PERIODIC_ADV_ADD_DEV_COMPLETE_EVT";
	case ESP_GAP_BLE_PERIODIC_ADV_REMOVE_DEV_COMPLETE_EVT: return "PERIODIC_ADV_REMOVE_DEV_COMPLETE_EVT";
	case ESP_GAP_BLE_PERIODIC_ADV_CLEAR_DEV_COMPLETE_EVT: return "PERIODIC_ADV_CLEAR_DEV_COMPLETE_EVT";
	case ESP_GAP_BLE_SET_EXT_SCAN_PARAMS_COMPLETE_EVT: return "SET_EXT_SCAN_PARAMS_COMPLETE_EVT";
	case ESP_GAP_BLE_EXT_SCAN_START_COMPLETE_EVT: return "EXT_SCAN_START_COMPLETE_EVT";
	case ESP_GAP_BLE_EXT_SCAN_STOP_COMPLETE_EVT: return "EXT_SCAN_STOP_COMPLETE_EVT";
	case ESP_GAP_BLE_PREFER_EXT_CONN_PARAMS_SET_COMPLETE_EVT: return "PREFER_EXT_CONN_PARAMS_SET_COMPLETE_EVT";
	case ESP_GAP_BLE_PHY_UPDATE_COMPLETE_EVT: return "PHY_UPDATE_COMPLETE_EVT";
	case ESP_GAP_BLE_EXT_ADV_REPORT_EVT: return "EXT_ADV_REPORT_EVT";
	case ESP_GAP_BLE_SCAN_TIMEOUT_EVT: return "SCAN_TIMEOUT_EVT";
	case ESP_GAP_BLE_ADV_TERMINATED_EVT: return "ADV_TERMINATED_EVT";
	case ESP_GAP_BLE_SCAN_REQ_RECEIVED_EVT: return "SCAN_REQ_RECEIVED_EVT";
	case ESP_GAP_BLE_CHANNEL_SELECT_ALGORITHM_EVT: return "CHANNEL_SELECT_ALGORITHM_EVT";
	case ESP_GAP_BLE_PERIODIC_ADV_REPORT_EVT: return "PERIODIC_ADV_REPORT_EVT";
	case ESP_GAP_BLE_PERIODIC_ADV_SYNC_LOST_EVT: return "PERIODIC_ADV_SYNC_LOST_EVT";
	case ESP_GAP_BLE_PERIODIC_ADV_SYNC_ESTAB_EVT: return "PERIODIC_ADV_SYNC_ESTAB_EVT";
	case ESP_GAP_BLE_SC_OOB_REQ_EVT: return "SC_OOB_REQ_EVT";
	case ESP_GAP_BLE_SC_CR_LOC_OOB_EVT: return "SC_CR_LOC_OOB_EVT";
	case ESP_GAP_BLE_GET_DEV_NAME_COMPLETE_EVT: return "GET_DEV_NAME_COMPLETE_EVT";
	case ESP_GAP_BLE_PERIODIC_ADV_RECV_ENABLE_COMPLETE_EVT: return "PERIODIC_ADV_RECV_ENABLE_COMPLETE_EVT";
	case ESP_GAP_BLE_PERIODIC_ADV_SYNC_TRANS_COMPLETE_EVT: return "PERIODIC_ADV_SYNC_TRANS_COMPLETE_EVT";
	case ESP_GAP_BLE_PERIODIC_ADV_SET_INFO_TRANS_COMPLETE_EVT: return "PERIODIC_ADV_SET_INFO_TRANS_COMPLETE_EVT";
	case ESP_GAP_BLE_SET_PAST_PARAMS_COMPLETE_EVT: return "SET_PAST_PARAMS_COMPLETE_EVT";
	case ESP_GAP_BLE_PERIODIC_ADV_SYNC_TRANS_RECV_EVT: return "PERIODIC_ADV_SYNC_TRANS_RECV_EVT";
	default: return "UNKNOWN";
	}
	// clang-format on
}
