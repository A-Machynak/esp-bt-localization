#pragma once

#include <cstdint>

#include <esp_gap_ble_api.h>

namespace Gap::Ble
{
/** Way too many BLE GAP events; we'll only use what we need **/

namespace Type
{

/// @brief BLE GAP callback parameter typedefs
/// @{
using AdvDataCmpl = esp_ble_gap_cb_param_t::ble_adv_data_cmpl_evt_param;
using ScanRspDataCmpl = esp_ble_gap_cb_param_t::ble_scan_rsp_data_cmpl_evt_param;
using ScanParamCmpl = esp_ble_gap_cb_param_t::ble_scan_param_cmpl_evt_param;
using ScanResult = esp_ble_gap_cb_param_t::ble_scan_result_evt_param;
using AdvDataRawCmpl = esp_ble_gap_cb_param_t::ble_adv_data_raw_cmpl_evt_param;
using ScanRspDataRawCmpl = esp_ble_gap_cb_param_t::ble_scan_rsp_data_raw_cmpl_evt_param;
using AdvStartCmpl = esp_ble_gap_cb_param_t::ble_adv_start_cmpl_evt_param;
using ScanStartCmpl = esp_ble_gap_cb_param_t::ble_scan_start_cmpl_evt_param;
using ScanStopCmpl = esp_ble_gap_cb_param_t::ble_scan_stop_cmpl_evt_param;
using AdvStopCmpl = esp_ble_gap_cb_param_t::ble_adv_stop_cmpl_evt_param;
using UpdateConn = esp_ble_gap_cb_param_t::ble_update_conn_params_evt_param;
/// @}
}  // namespace Type

using namespace Type;

/// @brief BLE GAP callback interface
class IGapCallback
{
public:
	virtual ~IGapCallback() {}

	// clang-format off
	virtual void GapBleAdvDataCmpl(const AdvDataCmpl &) {}          ///< ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT
	virtual void GapBleScanRspDataCmpl(const ScanRspDataCmpl &) {}  ///< ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT
	virtual void GapBleScanParamCmpl(const ScanParamCmpl &) {}      ///< ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT
	virtual void GapBleScanResult(const ScanResult &) {}            ///< ESP_GAP_BLE_SCAN_RESULT_EVT
	virtual void GapBleAdvDataRawCmpl(const AdvDataRawCmpl &) {}    ///< ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT
	virtual void GapBleScanRspDataRawCmpl(const ScanRspDataRawCmpl &) {} ///< ESP_GAP_BLE_SCAN_RSP_DATA_RAW_SET_COMPLETE_EVT
	virtual void GapBleAdvStartCmpl(const AdvStartCmpl &) {}    ///< ESP_GAP_BLE_ADV_START_COMPLETE_EVT
	virtual void GapBleScanStartCmpl(const ScanStartCmpl &) {}  ///< ESP_GAP_BLE_SCAN_START_COMPLETE_EVT
	virtual void GapBleAdvStopCmpl(const AdvStopCmpl &) {}      ///< ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT
	virtual void GapBleScanStopCmpl(const ScanStopCmpl &) {}    ///< ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT
	virtual void GapBleUpdateConn(const UpdateConn &) {}        ///< ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT
	// clang-format on
};

}  // namespace Gap::Ble
