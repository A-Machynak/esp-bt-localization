#pragma once

#include <esp_gattc_api.h>

namespace Gattc
{

namespace Type
{

/// @brief GATTc callback parameter typedefs
/// @{
using Reg = esp_ble_gattc_cb_param_t::gattc_reg_evt_param;
using Open = esp_ble_gattc_cb_param_t::gattc_open_evt_param;
using ReadChar = esp_ble_gattc_cb_param_t::gattc_read_char_evt_param;
using WriteChar = esp_ble_gattc_cb_param_t::gattc_write_evt_param;
using Close = esp_ble_gattc_cb_param_t::gattc_close_evt_param;
using SearchCmpl = esp_ble_gattc_cb_param_t::gattc_search_cmpl_evt_param;
using SearchRes = esp_ble_gattc_cb_param_t::gattc_search_res_evt_param;
using ReadDescr = esp_ble_gattc_cb_param_t::gattc_read_char_evt_param;
using WriteDescr = esp_ble_gattc_cb_param_t::gattc_write_evt_param;
using Notify = esp_ble_gattc_cb_param_t::gattc_notify_evt_param;
using PrepWrite = esp_ble_gattc_cb_param_t::gattc_write_evt_param;
using Exec = esp_ble_gattc_cb_param_t::gattc_exec_cmpl_evt_param;
using SrvcChg = esp_ble_gattc_cb_param_t::gattc_srvc_chg_evt_param;
using CfgMtu = esp_ble_gattc_cb_param_t::gattc_cfg_mtu_evt_param;
using Congest = esp_ble_gattc_cb_param_t::gattc_congest_evt_param;
using RegForNotify = esp_ble_gattc_cb_param_t::gattc_reg_for_notify_evt_param;
using UnregForNotify = esp_ble_gattc_cb_param_t::gattc_unreg_for_notify_evt_param;
using Connect = esp_ble_gattc_cb_param_t::gattc_connect_evt_param;
using Disconnect = esp_ble_gattc_cb_param_t::gattc_disconnect_evt_param;
using ReadMultiple = esp_ble_gattc_cb_param_t::gattc_read_char_evt_param;
using QueueFull = esp_ble_gattc_cb_param_t::gattc_queue_full_evt_param;
using SetAssoc = esp_ble_gattc_cb_param_t::gattc_set_assoc_addr_cmp_evt_param;
using GetAddrList = esp_ble_gattc_cb_param_t::gattc_get_addr_list_evt_param;
using DisSrvcCmpl = esp_ble_gattc_cb_param_t::gattc_dis_srvc_cmpl_evt_param;
using ReadMultiVar = esp_ble_gattc_cb_param_t::gattc_read_char_evt_param;
/// @}
}  // namespace Type

using namespace Type;

/// @brief GATTc callback interface
class IGattcCallback
{
public:
	virtual void GattcReg(const Reg &) {}                        ///< ESP_GATTC_REG_EVT
	virtual void GattcUnreg() {}                                 ///< ESP_GATTC_UNREG_EVT
	virtual void GattcOpen(const Open &) {}                      ///< ESP_GATTC_OPEN_EVT
	virtual void GattcReadChar(const ReadChar &) {}              ///< ESP_GATTC_READ_CHAR_EVT
	virtual void GattcWriteChar(const WriteChar &) {}            ///< ESP_GATTC_WRITE_CHAR_EVT
	virtual void GattcClose(const Close &) {}                    ///< ESP_GATTC_CLOSE_EVT
	virtual void GattcSearchCmpl(const SearchCmpl &) {}          ///< ESP_GATTC_SEARCH_CMPL_EVT
	virtual void GattcSearchRes(const SearchRes &) {}            ///< ESP_GATTC_SEARCH_RES_EVT
	virtual void GattcReadDescr(const ReadDescr &) {}            ///< ESP_GATTC_READ_DESCR_EVT
	virtual void GattcWriteDescr(const WriteDescr &) {}          ///< ESP_GATTC_WRITE_DESCR_EVT
	virtual void GattcNotify(const Notify &) {}                  ///< ESP_GATTC_NOTIFY_EVT
	virtual void GattcPrepWrite(const PrepWrite &) {}            ///< ESP_GATTC_PREP_WRITE_EVT
	virtual void GattcExec(const Exec &) {}                      ///< ESP_GATTC_EXEC_EVT
	virtual void GattcAcl() {}                                   ///< ESP_GATTC_ACL_EVT
	virtual void GattcCancelOpen() {}                            ///< ESP_GATTC_CANCEL_OPEN_EVT
	virtual void GattcSrvcChg(const SrvcChg &) {}                ///< ESP_GATTC_SRVC_CHG_EVT
	virtual void GattcEncCmplCb() {}                             ///< ESP_GATTC_ENC_CMPL_CB_EVT
	virtual void GattcCfgMtu(const CfgMtu &) {}                  ///< ESP_GATTC_CFG_MTU_EVT
	virtual void GattcAdvData() {}                               ///< ESP_GATTC_ADV_DATA_EVT
	virtual void GattcMultAdvEnb() {}                            ///< ESP_GATTC_MULT_ADV_ENB_EVT
	virtual void GattcMultAdvUpd() {}                            ///< ESP_GATTC_MULT_ADV_UPD_EVT
	virtual void GattcMultAdvData() {}                           ///< ESP_GATTC_MULT_ADV_DATA_EVT
	virtual void GattcMultAdvDis() {}                            ///< ESP_GATTC_MULT_ADV_DIS_EVT
	virtual void GattcCongest(const Congest &) {}                ///< ESP_GATTC_CONGEST_EVT
	virtual void GattcBthScanEnb() {}                            ///< ESP_GATTC_BTH_SCAN_ENB_EVT
	virtual void GattcBthScanCfg() {}                            ///< ESP_GATTC_BTH_SCAN_CFG_EVT
	virtual void GattcBthScanRd() {}                             ///< ESP_GATTC_BTH_SCAN_RD_EVT
	virtual void GattcBthScanThr() {}                            ///< ESP_GATTC_BTH_SCAN_THR_EVT
	virtual void GattcBthScanParam() {}                          ///< ESP_GATTC_BTH_SCAN_PARAM_EVT
	virtual void GattcBthScanDis() {}                            ///< ESP_GATTC_BTH_SCAN_DIS_EVT
	virtual void GattcScanFltCfg() {}                            ///< ESP_GATTC_SCAN_FLT_CFG_EVT
	virtual void GattcScanFltParam() {}                          ///< ESP_GATTC_SCAN_FLT_PARAM_EVT
	virtual void GattcScanFltStatus() {}                         ///< ESP_GATTC_SCAN_FLT_STATUS_EVT
	virtual void GattcAdvVsc() {}                                ///< ESP_GATTC_ADV_VSC_EVT
	virtual void GattcRegForNotify(const RegForNotify &) {}      ///< ESP_GATTC_REG_FOR_NOTIFY_EVT
	virtual void GattcUnregForNotify(const UnregForNotify &) {}  ///< ESP_GATTC_UNREG_FOR_NOTIFY_EVT
	virtual void GattcConnect(const Connect &) {}                ///< ESP_GATTC_CONNECT_EVT
	virtual void GattcDisconnect(const Disconnect &) {}          ///< ESP_GATTC_DISCONNECT_EVT
	virtual void GattcReadMultiple(const ReadMultiple &) {}      ///< ESP_GATTC_READ_MULTIPLE_EVT
	virtual void GattcQueueFull(const QueueFull &) {}            ///< ESP_GATTC_QUEUE_FULL_EVT
	virtual void GattcSetAssoc(const SetAssoc &) {}              ///< ESP_GATTC_SET_ASSOC_EVT
	virtual void GattcGetAddrList(const GetAddrList &) {}        ///< ESP_GATTC_GET_ADDR_LIST_EVT
	virtual void GattcDisSrvcCmpl(const DisSrvcCmpl &) {}        ///< ESP_GATTC_DIS_SRVC_CMPL_EVT
	virtual void GattcReadMultiVar(const ReadMultiVar &) {}      ///< ESP_GATTC_READ_MULTI_VAR_EVT
};

}  // namespace Gattc
