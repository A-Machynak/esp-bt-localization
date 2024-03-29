
#include "core/wrapper/gattc_wrapper.h"

#include <esp_log.h>

#include <functional>
#include <vector>

namespace
{

/// @brief Logger tag
static const char * TAG = "GATTc";

esp_bt_uuid_t ToUuid(std::span<std::uint8_t> data)
{
	esp_bt_uuid_t uuid;
	if (data.size() == 2) {
		uuid.len = 2;
		uuid.uuid.uuid16 = *reinterpret_cast<std::uint16_t *>(data.data());
	}
	else if (data.size() == 4) {
		uuid.len = 4;
		uuid.uuid.uuid32 = *reinterpret_cast<std::uint32_t *>(data.data());
	}
	else if (data.size() == 16) {
		uuid.len = 16;
		std::copy_n(data.begin(), 16, uuid.uuid.uuid128);
	}
	return uuid;
}

/// @brief It's ugly, but there isn't much we can do about this...
Gattc::Wrapper * _Wrapper;

static void GattcCallbackPassthrough(esp_gattc_cb_event_t event,
                                     esp_gatt_if_t gattIf,
                                     esp_ble_gattc_cb_param_t * param)
{
	// ESP_LOGI(TAG, "%s", ToString(event));
	_Wrapper->GattcCallback(event, gattIf, param);
}
}  // namespace

namespace Gattc
{

Wrapper::Wrapper()
{
	_Wrapper = this;
}

Wrapper::~Wrapper()
{
	for (auto & app : _apps) {
		esp_ble_gattc_app_unregister(app.GattIf);
	}
}

void Wrapper::RegisterApp(std::uint16_t appId, Gattc::IGattcCallback * callback)
{
	if (!_initialized) {
		esp_ble_gattc_register_callback(GattcCallbackPassthrough);
		_initialized = true;
	}

	_apps.emplace_back(appId, ESP_GATT_IF_NONE, callback);
	esp_err_t err = esp_ble_gattc_app_register(appId);
	if (err != ESP_OK) {
		ESP_LOGW(TAG, "Couldn't register app (%d)", err);
		_apps.pop_back();
	}
	else {
		ESP_LOGI(TAG, "App %d registered", appId);
	}
}

void Wrapper::UnregisterApp(std::uint16_t id)
{
	for (auto it = _apps.begin(); it != _apps.end(); it++) {
		if (it->AppId == id) {
			ESP_ERROR_CHECK(esp_ble_gattc_app_unregister(it->GattIf));
			_apps.erase(it);
			break;
		}
	}
}

void Wrapper::SetLocalMtu(std::uint16_t mtu)
{
	constexpr std::uint16_t DefaultMtu = ESP_GATT_DEF_BLE_MTU_SIZE;
	constexpr std::uint16_t MaxMtu = ESP_GATT_MAX_MTU_SIZE;
	esp_ble_gatt_set_local_mtu(std::clamp(mtu, DefaultMtu, MaxMtu));
}

void Wrapper::Connect(std::uint16_t appId, const Bt::Device & device)
{
	if (!device.IsBle()) {
		ESP_LOGE(TAG, "Can only connect to a BLE device!");
		return;
	}

	Connect(appId, device.Bda, std::get<Bt::BleSpecific>(device.Data).AddrType);
}

void Wrapper::Connect(std::uint16_t appId, const Mac & address, esp_ble_addr_type_t addrType)
{
	AppInfo * app = _GetAppInfo(appId);
	if (app == nullptr) {
		ESP_LOGW(TAG, "Application %d not found", appId);
		return;
	}

	// ESP-IDF keeps asking for non-const pointers. I don't care anymore. I'm copying it.
	// They are memcpy-ing it right away anyway. Really annoying.
	Mac addrCpy = address;
	esp_err_t err = esp_ble_gattc_open(app->GattIf, addrCpy.Addr.data(), addrType, true);
	if (err != ESP_OK) {
		ESP_LOGW(TAG, "Couldn't connect to device (%d)", err);
		app->Callback->GattcCancelOpen();
	}
	else {
		ESP_LOGI(TAG, "Connecting to device...");
	}
}

void Wrapper::Disconnect(std::uint16_t appId, std::uint16_t connId)
{
	AppInfo * app = _GetAppInfo(appId);
	if (app == nullptr) {
		ESP_LOGW(TAG, "Application %d not found", appId);
		return;
	}
	esp_ble_gattc_close(app->GattIf, connId);

	for (auto it = _conns.begin(); it != _conns.end(); it++) {
		if (it->AppId == appId && it->ConnId == connId) {
			_conns.erase(it);
			break;
		}
	}
}

void Wrapper::SearchServices(std::uint16_t appId, std::uint16_t connId)
{
	AppInfo * app = _GetAppInfo(appId);
	if (!app) {
		ESP_LOGW(TAG, "Application %d not found", appId);
		return;
	}
	esp_ble_gattc_search_service(app->GattIf, connId, nullptr);
}

void Wrapper::SearchServices(std::uint16_t appId,
                             std::uint16_t connId,
                             std::span<std::uint8_t> filter)
{
	AppInfo * app = _GetAppInfo(appId);
	if (!app) {
		ESP_LOGW(TAG, "Application %d not found", appId);
		return;
	}

	esp_bt_uuid_t uuid = ToUuid(filter);
	if (uuid.len == 0) {
		ESP_LOGW(TAG, "Invalid filter length. Ignoring");
		esp_ble_gattc_search_service(app->GattIf, connId, nullptr);
		return;
	}
	esp_ble_gattc_search_service(app->GattIf, connId, &uuid);
}

void Wrapper::WriteCharVal(std::uint16_t appId,
                           std::uint16_t connId,
                           std::uint16_t charHandle,
                           std::span<std::uint8_t> value)
{
	_WriteChar(appId, connId, charHandle, value, true);
}

void Wrapper::WriteCharDescr(std::uint16_t appId,
                             std::uint16_t connId,
                             std::uint16_t charHandle,
                             std::span<std::uint8_t> value)
{
	_WriteChar(appId, connId, charHandle, value, false);
}

void Wrapper::_WriteChar(std::uint16_t appId,
                         std::uint16_t connId,
                         std::uint16_t charHandle,
                         std::span<std::uint8_t> value,
                         bool writeValue)
{
	// "4.9.4 Write Long Characteristic Values
	// This sub-procedure is used to write a Characteristic Value to a server when the client knows
	// the Characteristic Value Handle but the length of the Characteristic Value is longer than can
	// be sent in a single Write Request Attribute Protocol message. The Attribute Protocol Prepare
	// Write Request and Execute Write Request are used to perform this sub-procedure. The Attribute
	// Handle parameter shall be set to the Characteristic Value Handle of the Characteristic Value
	// to be written. The Part Attribute Value parameter shall be set to the part of the Attribute
	// Value that is being written. The Value Offset parameter shall be the offset within the
	// Characteristic Value to be written. To write the complete Characteristic Value the offset
	// should be set to 0x0000 for the first Prepare Write Request. The offset for subsequent
	// Prepare Write Requests is the next octet that has yet to be written. The Prepare Write
	// Request is repeated until the complete Characteristic Value has been transferred, after which
	// an Executive Write Request is used to write the complete value."

	auto writeFn = writeValue ? esp_ble_gattc_write_char : esp_ble_gattc_write_char_descr;
	auto prepFn = writeValue ? esp_ble_gattc_prepare_write : esp_ble_gattc_prepare_write_char_descr;

	AppInfo * app = _GetAppInfo(appId);
	ConnectionInfo * conn = _GetConnInfo(appId, connId);
	if (!conn) {
		ESP_LOGW(TAG, "Connection not found (app %d conn %d)", appId, connId);
		return;
	}

	// ATT_MTU - 3
	const std::size_t maxDataSize = conn->Mtu - 3;
	if (value.size() < maxDataSize) {
		// Fits into MTU, simple write
		// TODO: encryption
		writeFn(app->GattIf, connId, charHandle, value.size(), value.data(),
		        ESP_GATT_WRITE_TYPE_NO_RSP, ESP_GATT_AUTH_REQ_NONE);
		return;
	}
	// Exceeds MTU, split into prepare-execute

	std::uint16_t offset = 0;
	const std::size_t fullPdus = maxDataSize / value.size();

	// Write N full PDUs
	for (std::size_t i = 0; i < fullPdus + 1; i++) {
		const int diff = value.size() - offset;
		if (diff <= 0) {
			break;
		}
		const std::uint16_t length = std::min(diff, (int)maxDataSize);

		if ((offset + length) > value.size()) {  // sanity check
			break;
		}

		// Prepare Write
		esp_err_t err = prepFn(app->GattIf, connId, charHandle, offset, length,
		                       value.data() + offset, ESP_GATT_AUTH_REQ_NONE);
		if (err != ESP_OK) {
			ESP_LOGW(TAG, "Prepare write failed (%d)", err);
			esp_ble_gattc_execute_write(app->GattIf, connId, false);
			break;  // Can't do much
		}
		offset += length;
	}

	// Execute Write
	esp_ble_gattc_execute_write(app->GattIf, connId, true);
}

const AppInfo * Wrapper::GetAppInfo(std::uint16_t appId) const
{
	auto it = std::find_if(_apps.begin(), _apps.end(),
	                       [appId](const AppInfo & app) { return app.AppId == appId; });
	return it != _apps.end() ? &*it : nullptr;
}

AppInfo * Wrapper::_GetAppInfo(std::uint16_t appId)
{
	return const_cast<AppInfo *>(static_cast<const Wrapper *>(this)->GetAppInfo(appId));
}

const ConnectionInfo * Wrapper::GetConnInfo(std::uint16_t appId, std::uint16_t connId) const
{
	auto it =
	    std::find_if(_conns.begin(), _conns.end(), [appId, connId](const ConnectionInfo & app) {
		    return app.ConnId == connId && app.AppId == appId;
	    });
	return it != _conns.end() ? &*it : nullptr;
}

ConnectionInfo * Wrapper::_GetConnInfo(std::uint16_t appId, std::uint16_t connId)
{
	return const_cast<ConnectionInfo *>(
	    static_cast<const Wrapper *>(this)->GetConnInfo(appId, connId));
}

void Wrapper::GattcCallback(esp_gattc_cb_event_t event,
                            esp_gatt_if_t gattIf,
                            esp_ble_gattc_cb_param_t * param)
{
	AppInfo * app = nullptr;
	if (event == ESP_GATTC_REG_EVT) {
		// Store IF
		app = _GetAppInfo(param->reg.app_id);
		if (app == nullptr) {
			ESP_LOGE(TAG, "Application %d not found", param->reg.app_id);
		}
		app->GattIf = gattIf;
	}
	else [[likely]] {
		// Find callback
		for (AppInfo & appInfo : _apps) {
			if (gattIf == appInfo.GattIf) {
				app = &appInfo;
				break;
			}
		}
	}

	if (app == nullptr) {
		return;  // Shouldn't really happen(?)
	}

	Gattc::IGattcCallback * cb = app->Callback;
	// clang-format off
	switch(event) {
	case ESP_GATTC_REG_EVT: cb->GattcReg(param->reg); break;
	case ESP_GATTC_UNREG_EVT: cb->GattcUnreg(); break;
	case ESP_GATTC_OPEN_EVT: {
		// Save connection
		if (param->open.status == ESP_GATT_OK) {
			_conns.emplace_back(app->AppId, param->open.conn_id, param->open.mtu, Mac(param->open.remote_bda));
		}
		cb->GattcOpen(param->open);
		break;
	}
	case ESP_GATTC_READ_CHAR_EVT: cb->GattcReadChar(param->read); break;
	case ESP_GATTC_WRITE_CHAR_EVT: cb->GattcWriteChar(param->write); break;
	case ESP_GATTC_CLOSE_EVT: {
		// Remove connection
		for (auto it = _conns.begin(); it != _conns.end(); it++) {
			const auto * aid = GetAppInfo(it->AppId);
			if (it->ConnId == param->close.conn_id && aid && aid->AppId == app->AppId) {
				_conns.erase(it);
				break;
			}
		}
		cb->GattcClose(param->close);
		break;
	}
	case ESP_GATTC_SEARCH_CMPL_EVT: cb->GattcSearchCmpl(param->search_cmpl); break;
	case ESP_GATTC_SEARCH_RES_EVT: cb->GattcSearchRes(param->search_res); break;
	case ESP_GATTC_READ_DESCR_EVT: cb->GattcReadDescr(param->read); break;
	case ESP_GATTC_WRITE_DESCR_EVT: cb->GattcWriteDescr(param->write); break;
	case ESP_GATTC_NOTIFY_EVT: cb->GattcNotify(param->notify); break;
	case ESP_GATTC_PREP_WRITE_EVT: cb->GattcPrepWrite(param->write); break;
	case ESP_GATTC_EXEC_EVT: cb->GattcExec(param->exec_cmpl); break;
	case ESP_GATTC_ACL_EVT: cb->GattcAcl(); break;
	case ESP_GATTC_CANCEL_OPEN_EVT: cb->GattcCancelOpen(); break;
	case ESP_GATTC_SRVC_CHG_EVT: cb->GattcSrvcChg(param->srvc_chg); break;
	case ESP_GATTC_ENC_CMPL_CB_EVT: cb->GattcEncCmplCb(); break;
	case ESP_GATTC_CFG_MTU_EVT: cb->GattcCfgMtu(param->cfg_mtu); break;
	case ESP_GATTC_ADV_DATA_EVT: cb->GattcAdvData(); break;
	case ESP_GATTC_MULT_ADV_ENB_EVT: cb->GattcMultAdvEnb(); break;
	case ESP_GATTC_MULT_ADV_UPD_EVT: cb->GattcMultAdvUpd(); break;
	case ESP_GATTC_MULT_ADV_DATA_EVT: cb->GattcMultAdvData(); break;
	case ESP_GATTC_MULT_ADV_DIS_EVT: cb->GattcMultAdvDis(); break;
	case ESP_GATTC_CONGEST_EVT: cb->GattcCongest(param->congest); break;
	case ESP_GATTC_BTH_SCAN_ENB_EVT: cb->GattcBthScanEnb(); break;
	case ESP_GATTC_BTH_SCAN_CFG_EVT: cb->GattcBthScanCfg(); break;
	case ESP_GATTC_BTH_SCAN_RD_EVT: cb->GattcBthScanRd(); break;
	case ESP_GATTC_BTH_SCAN_THR_EVT: cb->GattcBthScanThr(); break;
	case ESP_GATTC_BTH_SCAN_PARAM_EVT: cb->GattcBthScanParam(); break;
	case ESP_GATTC_BTH_SCAN_DIS_EVT: cb->GattcBthScanDis(); break;
	case ESP_GATTC_SCAN_FLT_CFG_EVT: cb->GattcScanFltCfg(); break;
	case ESP_GATTC_SCAN_FLT_PARAM_EVT: cb->GattcScanFltParam(); break;
	case ESP_GATTC_SCAN_FLT_STATUS_EVT: cb->GattcScanFltStatus(); break;
	case ESP_GATTC_ADV_VSC_EVT: cb->GattcAdvVsc(); break;
	case ESP_GATTC_REG_FOR_NOTIFY_EVT: cb->GattcRegForNotify(param->reg_for_notify); break;
	case ESP_GATTC_UNREG_FOR_NOTIFY_EVT: cb->GattcUnregForNotify(param->unreg_for_notify); break;
	case ESP_GATTC_CONNECT_EVT: cb->GattcConnect(param->connect); break;
	case ESP_GATTC_DISCONNECT_EVT: cb->GattcDisconnect(param->disconnect); break;
	case ESP_GATTC_READ_MULTIPLE_EVT: cb->GattcReadMultiple(param->read); break;
	case ESP_GATTC_QUEUE_FULL_EVT: cb->GattcQueueFull(param->queue_full); break;
	case ESP_GATTC_SET_ASSOC_EVT: cb->GattcSetAssoc(param->set_assoc_cmp); break;
	case ESP_GATTC_GET_ADDR_LIST_EVT: cb->GattcGetAddrList(param->get_addr_list); break;
	case ESP_GATTC_DIS_SRVC_CMPL_EVT: cb->GattcDisSrvcCmpl(param->dis_srvc_cmpl); break;
	case ESP_GATTC_READ_MULTI_VAR_EVT: cb->GattcReadMultiVar(param->read); break;
	default: break;
	}
	// clang-format on
}

}  // namespace Gattc

const char * ToString(const esp_gattc_cb_event_t event)
{
	// clang-format off
	switch (event) {
	case ESP_GATTC_REG_EVT: return "REG_EVT";
	case ESP_GATTC_UNREG_EVT: return "UNREG_EVT";
	case ESP_GATTC_OPEN_EVT: return "OPEN_EVT";
	case ESP_GATTC_READ_CHAR_EVT: return "READ_CHAR_EVT";
	case ESP_GATTC_WRITE_CHAR_EVT: return "WRITE_CHAR_EVT";
	case ESP_GATTC_CLOSE_EVT: return "CLOSE_EVT";
	case ESP_GATTC_SEARCH_CMPL_EVT: return "SEARCH_CMPL_EVT";
	case ESP_GATTC_SEARCH_RES_EVT: return "SEARCH_RES_EVT";
	case ESP_GATTC_READ_DESCR_EVT: return "READ_DESCR_EVT";
	case ESP_GATTC_WRITE_DESCR_EVT: return "WRITE_DESCR_EVT";
	case ESP_GATTC_NOTIFY_EVT: return "NOTIFY_EVT";
	case ESP_GATTC_PREP_WRITE_EVT: return "PREP_WRITE_EVT";
	case ESP_GATTC_EXEC_EVT: return "EXEC_EVT";
	case ESP_GATTC_ACL_EVT: return "ACL_EVT";
	case ESP_GATTC_CANCEL_OPEN_EVT: return "CANCEL_OPEN_EVT";
	case ESP_GATTC_SRVC_CHG_EVT: return "SRVC_CHG_EVT";
	case ESP_GATTC_ENC_CMPL_CB_EVT: return "ENC_CMPL_CB_EVT";
	case ESP_GATTC_CFG_MTU_EVT: return "CFG_MTU_EVT";
	case ESP_GATTC_ADV_DATA_EVT: return "ADV_DATA_EVT";
	case ESP_GATTC_MULT_ADV_ENB_EVT: return "MULT_ADV_ENB_EVT";
	case ESP_GATTC_MULT_ADV_UPD_EVT: return "MULT_ADV_UPD_EVT";
	case ESP_GATTC_MULT_ADV_DATA_EVT: return "MULT_ADV_DATA_EVT";
	case ESP_GATTC_MULT_ADV_DIS_EVT: return "MULT_ADV_DIS_EVT";
	case ESP_GATTC_CONGEST_EVT: return "CONGEST_EVT";
	case ESP_GATTC_BTH_SCAN_ENB_EVT: return "BTH_SCAN_ENB_EVT";
	case ESP_GATTC_BTH_SCAN_CFG_EVT: return "BTH_SCAN_CFG_EVT";
	case ESP_GATTC_BTH_SCAN_RD_EVT: return "BTH_SCAN_RD_EVT";
	case ESP_GATTC_BTH_SCAN_THR_EVT: return "BTH_SCAN_THR_EVT";
	case ESP_GATTC_BTH_SCAN_PARAM_EVT: return "BTH_SCAN_PARAM_EVT";
	case ESP_GATTC_BTH_SCAN_DIS_EVT: return "BTH_SCAN_DIS_EVT";
	case ESP_GATTC_SCAN_FLT_CFG_EVT: return "SCAN_FLT_CFG_EVT";
	case ESP_GATTC_SCAN_FLT_PARAM_EVT: return "SCAN_FLT_PARAM_EVT";
	case ESP_GATTC_SCAN_FLT_STATUS_EVT: return "SCAN_FLT_STATUS_EVT";
	case ESP_GATTC_ADV_VSC_EVT: return "ADV_VSC_EVT";
	case ESP_GATTC_REG_FOR_NOTIFY_EVT: return "REG_FOR_NOTIFY_EVT";
	case ESP_GATTC_UNREG_FOR_NOTIFY_EVT: return "UNREG_FOR_NOTIFY_EVT";
	case ESP_GATTC_CONNECT_EVT: return "CONNECT_EVT";
	case ESP_GATTC_DISCONNECT_EVT: return "DISCONNECT_EVT";
	case ESP_GATTC_READ_MULTIPLE_EVT: return "READ_MULTIPLE_EVT";
	case ESP_GATTC_QUEUE_FULL_EVT: return "QUEUE_FULL_EVT";
	case ESP_GATTC_SET_ASSOC_EVT: return "SET_ASSOC_EVT";
	case ESP_GATTC_GET_ADDR_LIST_EVT: return "GET_ADDR_LIST_EVT";
	case ESP_GATTC_DIS_SRVC_CMPL_EVT: return "DIS_SRVC_CMPL_EVT";
	case ESP_GATTC_READ_MULTI_VAR_EVT: return "READ_MULTI_VAR_EVT";
	default: return "Unknown";
	}
	// clang-format on
}
