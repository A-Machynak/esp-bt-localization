
#include "core/wrapper/gatts_wrapper.h"

#include <esp_gatt_common_api.h>
#include <esp_log.h>

#include <algorithm>
#include <vector>

namespace
{
/// @brief Logger tag
static const char * TAG = "GATTs";

/// @brief It's ugly, but there isn't much we can do about this...
static Gatts::Wrapper * _Wrapper = nullptr;

static void GattsCallbackPassthrough(esp_gatts_cb_event_t event,
                                     esp_gatt_if_t gattIf,
                                     esp_ble_gatts_cb_param_t * param)
{
	ESP_LOGI(TAG, "%s", ToString(event));
	_Wrapper->GattsCallback(event, gattIf, param);
}
}  // namespace

namespace Gatts
{

Wrapper::Wrapper(bool legacyWrite)
    : _legacyWrite(legacyWrite)
{
	_Wrapper = this;
}

Wrapper::~Wrapper()
{
	for (auto & app : _apps) {
		esp_ble_gatts_app_unregister(app.AppId);
	}
}

void Wrapper::RegisterApp(std::uint16_t appId, Gatts::IGattsCallback * callback)
{
	if (!_initialized) {
		ESP_ERROR_CHECK(esp_ble_gatts_register_callback(GattsCallbackPassthrough));
		_initialized = true;
	}

	_apps.emplace_back(appId, ESP_GATT_IF_NONE, callback);
	esp_err_t err = esp_ble_gatts_app_register(appId);
	if (err != ESP_OK) {
		ESP_LOGW(TAG, "Couldn't register app (%d)", err);
		_apps.pop_back();
	}
	ESP_LOGI(TAG, "App %d registered", appId);
}

void Wrapper::UnregisterApp(std::uint16_t appId)
{
	for (auto it = _apps.begin(); it != _apps.end(); it++) {
		if (it->AppId == appId) {
			esp_ble_gatts_app_unregister(appId);
			_apps.erase(it);
			return;
		}
	}
	ESP_LOGE(TAG, "Invalid appId");
}

void Wrapper::Close(std::uint16_t appId, std::uint16_t connId)
{
	const AppInfo * app = GetAppInfo(appId);
	if (!app) {
		ESP_LOGE(TAG, "Invalid appId");
		return;
	}
	esp_ble_gatts_close(app->GattIf, connId);
}

void Wrapper::CreateAttributeTable(std::uint16_t appId,
                                   const Gatt::AttributeTable & attributeTable,
                                   const std::vector<std::size_t> & serviceIndices)
{
	AppInfo * appInfo = _GetAppInfo(appId);
	if (!appInfo) {
		ESP_LOGE(TAG, "Invalid appId");
		return;
	}

	appInfo->ServiceIndices = serviceIndices;

	if (appInfo->GattIf != ESP_GATT_IF_NONE) {
		const auto & db = attributeTable.Db;
		ESP_ERROR_CHECK(esp_ble_gatts_create_attr_tab(db.data(), appInfo->GattIf, db.size(), 0));
		ESP_LOGI(TAG, "Created attribute table (%d attributes)", db.size());
		//! service instance id?
	}
	else {
		ESP_LOGE(TAG, "Couldn't create attribute table");
	}
}

void Wrapper::SetLocalMtu(std::uint16_t mtu)
{
	constexpr std::uint16_t DefaultMtu = ESP_GATT_DEF_BLE_MTU_SIZE;
	constexpr std::uint16_t MaxMtu = ESP_GATT_MAX_MTU_SIZE;
	esp_ble_gatt_set_local_mtu(std::clamp(mtu, DefaultMtu, MaxMtu));
}

const AppInfo * Wrapper::GetAppInfo(std::uint16_t appId) const
{
	for (const AppInfo & app : _apps) {
		if (app.AppId == appId) {
			return &app;
		}
	}
	return nullptr;
}

AppInfo * Wrapper::_GetAppInfo(std::uint16_t appId)
{
	for (AppInfo & app : _apps) {
		if (app.AppId == appId) {
			return &app;
		}
	}
	return nullptr;
}

void Wrapper::GattsCallback(esp_gatts_cb_event_t event,
                            esp_gatt_if_t gattIf,
                            esp_ble_gatts_cb_param_t * param)
{
	AppInfo * app = nullptr;
	if (event == ESP_GATTS_REG_EVT) {
		// Store IF
		app = _GetAppInfo(param->reg.app_id);
		if (app == nullptr) {
			ESP_LOGE(TAG, "AppInfo not found!");
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

	Gatts::IGattsCallback * cb = app->Callback;
	// clang-format off
	switch (event) {
	case ESP_GATTS_REG_EVT: cb->GattsRegister(param->reg); break;
	case ESP_GATTS_READ_EVT: cb->GattsRead(param->read); break;
	case ESP_GATTS_WRITE_EVT: _GattsWrite(*app, param->write); break;
	case ESP_GATTS_EXEC_WRITE_EVT: _GattsExecWrite(*app, param->exec_write); break;
	case ESP_GATTS_MTU_EVT: cb->GattsMtu(param->mtu); break;
	case ESP_GATTS_CONF_EVT: cb->GattsConf(param->conf); break;
	case ESP_GATTS_UNREG_EVT: cb->GattsUnreg(); break;
	case ESP_GATTS_CREATE_EVT: cb->GattsCreate(param->create); break;
	case ESP_GATTS_ADD_INCL_SRVC_EVT: cb->GattsAddInclSrvc(param->add_incl_srvc); break;
	case ESP_GATTS_ADD_CHAR_EVT: cb->GattsAddChar(param->add_char); break;
	case ESP_GATTS_ADD_CHAR_DESCR_EVT: cb->GattsAddCharDescr(param->add_char_descr); break;
	case ESP_GATTS_DELETE_EVT: cb->GattsDelete(param->del); break;
	case ESP_GATTS_START_EVT: cb->GattsStart(param->start); break;
	case ESP_GATTS_STOP_EVT: cb->GattsStop(param->stop); break;
	case ESP_GATTS_CONNECT_EVT: {
		_conns.emplace_back(app->AppId, param->connect.conn_id, Mac(param->connect.remote_bda));
		cb->GattsConnect(param->connect);
		break;
	}
	case ESP_GATTS_DISCONNECT_EVT: {
		// Remove connection
		for (auto it = _conns.begin(); it != _conns.end(); it++) {
			const auto * aid = GetAppInfo(it->AppId);
			if (it->ConnId == param->close.conn_id && aid && aid->AppId == app->AppId) {
				_conns.erase(it);
				break;
			}
		}
		cb->GattsDisconnect(param->disconnect);
		break;
	}
	case ESP_GATTS_OPEN_EVT: cb->GattsOpen(param->open); break;
	case ESP_GATTS_CANCEL_OPEN_EVT: cb->GattsCancelOpen(param->cancel_open); break;
	case ESP_GATTS_CLOSE_EVT: cb->GattsClose(param->close); break;
	case ESP_GATTS_LISTEN_EVT: cb->GattsListen(); break;
	case ESP_GATTS_CONGEST_EVT: cb->GattsCongest(param->congest); break;
	case ESP_GATTS_RESPONSE_EVT: cb->GattsResponse(param->rsp); break;
	case ESP_GATTS_CREAT_ATTR_TAB_EVT: {
		app->GattHandles.resize(param->add_attr_tab.num_handle);
		std::copy_n(param->add_attr_tab.handles, param->add_attr_tab.num_handle, app->GattHandles.begin());
		for (auto idx : app->ServiceIndices) {    // start all defined services
			ESP_LOGI(TAG, "Starting service %d (handle %d).", idx, app->GattHandles.at(idx));
			esp_ble_gatts_start_service(app->GattHandles.at(idx));
		}
		cb->GattsCreateAttrTab(param->add_attr_tab);
		break;
	}
	case ESP_GATTS_SET_ATTR_VAL_EVT: cb->GattsSetAttrVal(param->set_attr_val); break;
	case ESP_GATTS_SEND_SERVICE_CHANGE_EVT: cb->GattsSendSrvcChange(param->service_change); break;
	default: break;
	}
	// clang-format on
}

void Wrapper::_GattsWrite(AppInfo & app, const Gatts::Type::Write & p)
{
	if (_legacyWrite || !p.is_prep) {
		// Single write
		app.Callback->GattsWrite(p);
		return;
	}

	// Find transaction
	std::vector<Transaction>::iterator it = _transactions.begin();
	for (; it != _transactions.end(); it++) {
		if (app.AppId == it->AppId && p.conn_id == it->ConnId) {
			break;
		}
	}

	// Sanity check
	if ((it != _transactions.end()) != (p.offset == 0)) [[unlikely]] {
		// Cancel - invalid state
		if (p.offset == 0) {
			ESP_LOGW(TAG, "Offset is 0, but transaction already exists?");
		}
		else {
			ESP_LOGW(TAG, "Transaction doesn't exist, but offset isn't 0?");
		}
		_transactions.erase(it);
		return;
	}

	if (it == _transactions.end()) {
		// Start of transaction
		_transactions.emplace_back(app.AppId, p.conn_id, p.handle);
		it = std::prev(_transactions.end());
	}

	// Copy transaction data
	std::copy_n(p.value, p.len, std::back_inserter(it->WriteData));

	if (p.need_rsp) {
		// Send response
		esp_ble_gatts_send_response(app.GattIf, p.conn_id, p.trans_id, ESP_GATT_OK, nullptr);
	}
}

void Wrapper::_GattsExecWrite(AppInfo & app, const Gatts::Type::ExecWrite & p)
{
	if (_legacyWrite) [[unlikely]] {
		app.Callback->GattsExecWrite(p);
		return;
	}

	// Find transaction
	std::vector<Transaction>::iterator it = _transactions.begin();
	for (; it != _transactions.end(); it++) {
		if (it->AppId == app.AppId && it->ConnId == p.conn_id) {
			break;
		}
	}

	// Sanity check
	if (it == _transactions.end()) [[unlikely]] {
		ESP_LOGW(TAG, "Exec write for non-existing transaction (app %d, conn %d, trans %lu)",
		         app.AppId, p.conn_id, p.trans_id);
		return;
	}

	if (p.exec_write_flag != ESP_GATT_PREP_WRITE_EXEC) [[unlikely]] {
		// Cancel transaction
		_transactions.erase(it);
		return;
	}

	// Dummy write
	Gatts::Type::Write write{.conn_id = p.conn_id,
	                         .trans_id = p.trans_id,
	                         .bda = {p.bda[0], p.bda[1], p.bda[2], p.bda[3], p.bda[4], p.bda[5]},
	                         .handle = it->Handle,
	                         .offset = 0,
	                         .need_rsp = false,
	                         .is_prep = false,
	                         .len = (uint16_t)it->WriteData.size(),
	                         .value = it->WriteData.data()};
	app.Callback->GattsWrite(std::move(write));

	// Delete transaction after finishing
	_transactions.erase(it);
}

}  // namespace Gatts

const char * ToString(const esp_gatts_cb_event_t event)
{
	// clang-format off
	switch (event) {
	case ESP_GATTS_REG_EVT: return "REG_EVT";
	case ESP_GATTS_READ_EVT: return "READ_EVT";
	case ESP_GATTS_WRITE_EVT: return "WRITE_EVT";
	case ESP_GATTS_EXEC_WRITE_EVT: return "EXEC_WRITE_EVT";
	case ESP_GATTS_MTU_EVT: return "MTU_EVT";
	case ESP_GATTS_CONF_EVT: return "CONF_EVT";
	case ESP_GATTS_UNREG_EVT: return "UNREG_EVT";
	case ESP_GATTS_CREATE_EVT: return "CREATE_EVT";
	case ESP_GATTS_ADD_INCL_SRVC_EVT: return "ADD_INCL_SRVC_EVT";
	case ESP_GATTS_ADD_CHAR_EVT: return "ADD_CHAR_EVT";
	case ESP_GATTS_ADD_CHAR_DESCR_EVT: return "ADD_CHAR_DESCR_EVT";
	case ESP_GATTS_DELETE_EVT: return "DELETE_EVT";
	case ESP_GATTS_START_EVT: return "START_EVT";
	case ESP_GATTS_STOP_EVT: return "STOP_EVT";
	case ESP_GATTS_CONNECT_EVT: return "CONNECT_EVT";
	case ESP_GATTS_DISCONNECT_EVT: return "DISCONNECT_EVT";
	case ESP_GATTS_OPEN_EVT: return "OPEN_EVT";
	case ESP_GATTS_CANCEL_OPEN_EVT: return "CANCEL_OPEN_EVT";
	case ESP_GATTS_CLOSE_EVT: return "CLOSE_EVT";
	case ESP_GATTS_LISTEN_EVT: return "LISTEN_EVT";
	case ESP_GATTS_CONGEST_EVT: return "CONGEST_EVT";
	case ESP_GATTS_RESPONSE_EVT: return "RESPONSE_EVT";
	case ESP_GATTS_CREAT_ATTR_TAB_EVT: return "CREAT_ATTR_TAB_EVT";
	case ESP_GATTS_SET_ATTR_VAL_EVT: return "SET_ATTR_VAL_EVT";
	case ESP_GATTS_SEND_SERVICE_CHANGE_EVT: return "SEND_SERVICE_CHANGE_EVT";
	default: return "UNKNOWN";
	}
	// clang-format on
}