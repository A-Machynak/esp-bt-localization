#pragma once

#include <esp_gatts_api.h>
#include <string>

namespace Gatts
{

namespace Type
{

/// @brief GATTS callback parameter typedefs
/// @{
using Register = esp_ble_gatts_cb_param_t::gatts_reg_evt_param;
using Read = esp_ble_gatts_cb_param_t::gatts_read_evt_param;
using Write = esp_ble_gatts_cb_param_t::gatts_write_evt_param;
using ExecWrite = esp_ble_gatts_cb_param_t::gatts_exec_write_evt_param;
using Mtu = esp_ble_gatts_cb_param_t::gatts_mtu_evt_param;
using Conf = esp_ble_gatts_cb_param_t::gatts_conf_evt_param;
using Create = esp_ble_gatts_cb_param_t::gatts_create_evt_param;
using AddInclSrvc = esp_ble_gatts_cb_param_t::gatts_add_incl_srvc_evt_param;
using AddChar = esp_ble_gatts_cb_param_t::gatts_add_char_evt_param;
using AddCharDesc = esp_ble_gatts_cb_param_t::gatts_add_char_descr_evt_param;
using Delete = esp_ble_gatts_cb_param_t::gatts_delete_evt_param;
using Start = esp_ble_gatts_cb_param_t::gatts_start_evt_param;
using Stop = esp_ble_gatts_cb_param_t::gatts_stop_evt_param;
using Connect = esp_ble_gatts_cb_param_t::gatts_connect_evt_param;
using Disconnect = esp_ble_gatts_cb_param_t::gatts_disconnect_evt_param;
using Open = esp_ble_gatts_cb_param_t::gatts_open_evt_param;
using Cancel = esp_ble_gatts_cb_param_t::gatts_cancel_open_evt_param;
using Close = esp_ble_gatts_cb_param_t::gatts_close_evt_param;
using Congest = esp_ble_gatts_cb_param_t::gatts_congest_evt_param;
using Response = esp_ble_gatts_cb_param_t::gatts_rsp_evt_param;
using AddAttrTab = esp_ble_gatts_cb_param_t::gatts_add_attr_tab_evt_param;
using SetAttrVal = esp_ble_gatts_cb_param_t::gatts_set_attr_val_evt_param;
using SendSrvcChange = esp_ble_gatts_cb_param_t::gatts_send_service_change_evt_param;
/// }

}  // namespace Type

using namespace Type;

/// @brief GATTs callback interface.
class IGattsCallback
{
public:
	virtual ~IGattsCallback() {}

	// clang-format off
	virtual void GattsRegister(const Register &) {}              ///< ESP_GATTS_REG_EVT
	virtual void GattsRead(const Read &) {}                      ///< ESP_GATTS_READ_EVT
	virtual void GattsWrite(const Write &) {}                    ///< ESP_GATTS_WRITE_EVT
	virtual void GattsExecWrite(const ExecWrite &) {}            ///< ESP_GATTS_EXEC_WRITE_EVT
	virtual void GattsMtu(const Mtu &) {}                        ///< ESP_GATTS_MTU_EVT
	virtual void GattsConf(const Conf &) {}                      ///< ESP_GATTS_CONF_EVT
	virtual void GattsUnreg() {}                                 ///< ESP_GATTS_UNREG_EVT
	virtual void GattsCreate(const Create &) {}                  ///< ESP_GATTS_CREATE_EVT
	virtual void GattsAddInclSrvc(const AddInclSrvc &) {}        ///< ESP_GATTS_ADD_INCL_SRVC_EVT
	virtual void GattsAddChar(const AddChar &) {}                ///< ESP_GATTS_ADD_CHAR_EVT
	virtual void GattsAddCharDescr(const AddCharDesc &) {}       ///< ESP_GATTS_ADD_CHAR_DESCR_EVT
	virtual void GattsDelete(const Delete &) {}                  ///< ESP_GATTS_DELETE_EVT
	virtual void GattsStart(const Start &) {}                    ///< ESP_GATTS_START_EVT
	virtual void GattsStop(const Stop &) {}                      ///< ESP_GATTS_STOP_EVT
	virtual void GattsConnect(const Connect &) {}                ///< ESP_GATTS_CONNECT_EVT
	virtual void GattsDisconnect(const Disconnect &) {}          ///< ESP_GATTS_DISCONNECT_EVT
	virtual void GattsOpen(const Open &) {}                      ///< ESP_GATTS_OPEN_EVT
	virtual void GattsCancelOpen(const Cancel &) {}              ///< ESP_GATTS_CANCEL_OPEN_EVT
	virtual void GattsClose(const Close &) {}                    ///< ESP_GATTS_CLOSE_EVT
	virtual void GattsListen() {}                                ///< ESP_GATTS_LISTEN_EVT
	virtual void GattsCongest(const Congest &) {}                ///< ESP_GATTS_CONGEST_EVT
	virtual void GattsResponse(const Response &) {}              ///< ESP_GATTS_RESPONSE_EVT
	virtual void GattsCreateAttrTab(const AddAttrTab &) {}       ///< ESP_GATTS_CREAT_ATTR_TAB_EVT
	virtual void GattsSetAttrVal(const SetAttrVal &) {}          ///< ESP_GATTS_SET_ATTR_VAL_EVT
	virtual void GattsSendSrvcChange(const SendSrvcChange &) {}  ///< ESP_GATTS_SEND_SERVICE_CHANGE_EVT
	// clang-format on
};

}  // namespace Gatts

std::string ToString(const Gatts::Type::Write & p);
