#include "core/wrapper/gap_bt_wrapper.h"

#include <esp_log.h>

namespace
{
/// @brief Logger tag
static const char * TAG = "GAP_BT";

}  // namespace

#ifdef CONFIG_BT_CLASSIC_ENABLED
#include <algorithm>
#include <cstdint>
#include <vector>

namespace
{

/// @brief :(
static Gap::Bt::Wrapper * _Wrapper;

static void BtGapCallbackPassthrough(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t * param)
{
	ESP_LOGV(TAG, "%s", ToString(event));
	_Wrapper->BtGapCallback(event, param);
}
}  // namespace

namespace Gap::Bt
{
Wrapper::Wrapper(IGapCallback * callback)
    : _callback(callback)
{
	_Wrapper = this;
}

void Wrapper::Init()
{
	ESP_ERROR_CHECK(esp_bt_gap_register_callback(BtGapCallbackPassthrough));
}

Wrapper::~Wrapper() {}

void Wrapper::SetScanMode(ConnectionMode connectionMode, DiscoveryMode discoveryMode)
{
	ESP_ERROR_CHECK(esp_bt_gap_set_scan_mode(connectionMode, discoveryMode));
}

void Wrapper::StartDiscovery(InquiryMode inquiryMode, float time)
{
	_discoveryTime = std::max(time, 0.0f);
	_StartDiscoveryImpl(inquiryMode);
}

void Wrapper::_StartDiscoveryImpl(InquiryMode inquiryMode)
{
	constexpr std::uint8_t MaxTime = 0x30;
	constexpr float SecondsPerValue = 1.28f;
	constexpr float MaxTimeF = MaxTime * SecondsPerValue;

	if (_discoveryTime == DiscoverForever) {
		ESP_ERROR_CHECK(esp_bt_gap_start_discovery(inquiryMode, MaxTime, 0));
	}
	else if (_discoveryTime >= MaxTimeF) {
		ESP_ERROR_CHECK(esp_bt_gap_start_discovery(inquiryMode, MaxTime, 0));
		_discoveryTime -= MaxTimeF;
	}
	else {
		const auto div = static_cast<std::uint8_t>(_discoveryTime / SecondsPerValue);
		const std::uint8_t t = std::clamp(div, (std::uint8_t)1, MaxTime);
		ESP_ERROR_CHECK(esp_bt_gap_start_discovery(inquiryMode, t, 0));
		_discoveryTime = 0.0f;
	}
	ESP_LOGI(TAG, "Discovery started");
}

void Wrapper::StopDiscovery()
{
	if (_isDiscovering) {
		ESP_ERROR_CHECK(esp_bt_gap_cancel_discovery());
		ESP_LOGI(TAG, "Discovery stopped");
	}
}

void Wrapper::BtGapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t * param)
{
	if (!_callback) {
		return;
	}

	// clang-format off
	switch (event) {
	case ESP_BT_GAP_DISC_RES_EVT: _callback->GapBtDiscRes(param->disc_res); break;
	case ESP_BT_GAP_DISC_STATE_CHANGED_EVT: {
		_isDiscovering = (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED);
		_callback->GapBtDiscStateChanged(param->disc_st_chg);
		break;
	}
	case ESP_BT_GAP_RMT_SRVCS_EVT: _callback->GapBtRmtSrvcs(param->rmt_srvcs); break;
	case ESP_BT_GAP_RMT_SRVC_REC_EVT: _callback->GapBtRmtSrvcRec(param->rmt_srvc_rec); break;
	case ESP_BT_GAP_AUTH_CMPL_EVT: _callback->GapBtAuthCmpl(param->auth_cmpl); break;
	case ESP_BT_GAP_PIN_REQ_EVT: _callback->GapBtPinReq(param->pin_req); break;
	case ESP_BT_GAP_CFM_REQ_EVT: _callback->GapBtCfmReq(param->cfm_req); break;
	case ESP_BT_GAP_KEY_NOTIF_EVT: _callback->GapBtKeyNotif(param->key_notif); break;
	case ESP_BT_GAP_KEY_REQ_EVT: _callback->GapBtKeyReq(param->key_req); break;
	case ESP_BT_GAP_READ_RSSI_DELTA_EVT: _callback->GapBtReadRssiDelta(param->read_rssi_delta); break;
	case ESP_BT_GAP_CONFIG_EIR_DATA_EVT: _callback->GapBtConfigEirData(param->config_eir_data); break;
	case ESP_BT_GAP_SET_AFH_CHANNELS_EVT: _callback->GapBtSetAfhChannels(param->set_afh_channels); break;
	case ESP_BT_GAP_READ_REMOTE_NAME_EVT: _callback->GapBtReadRemoteName(param->read_rmt_name); break;
	case ESP_BT_GAP_MODE_CHG_EVT: _callback->GapBtModeChg(param->mode_chg); break;
	case ESP_BT_GAP_REMOVE_BOND_DEV_COMPLETE_EVT: _callback->GapBtRemoveBondDevCmpl(param->remove_bond_dev_cmpl); break;
	case ESP_BT_GAP_QOS_CMPL_EVT: _callback->GapBtQosCmpl(param->qos_cmpl); break;
	case ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT: _callback->GapBtAclConnCmplStat(param->acl_conn_cmpl_stat); break;
	case ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT: _callback->GapBtAclDisconnCmplStat(param->acl_disconn_cmpl_stat); break;
	default: break;
	}
	// clang-format on
}
}  // namespace Gap::Bt

#else

namespace Gap::Bt
{
Wrapper::Wrapper([[maybe_unused]] IGapCallback * callback)
{
	ESP_LOGI(TAG, "BT Classic disabled. Classic devices can't be scanned");
}
}  // namespace Gap::Bt
#endif

const char * ToString(const esp_bt_gap_cb_event_t event)
{
	// clang-format off
	switch (event) {
	case ESP_BT_GAP_DISC_RES_EVT: return "DISC_RES_EVT";
	case ESP_BT_GAP_DISC_STATE_CHANGED_EVT: return "DISC_STATE_CHANGED_EVT";
	case ESP_BT_GAP_RMT_SRVCS_EVT: return "RMT_SRVCS_EVT";
	case ESP_BT_GAP_RMT_SRVC_REC_EVT: return "RMT_SRVC_REC_EVT";
	case ESP_BT_GAP_AUTH_CMPL_EVT: return "AUTH_CMPL_EVT";
	case ESP_BT_GAP_PIN_REQ_EVT: return "PIN_REQ_EVT";
	case ESP_BT_GAP_CFM_REQ_EVT: return "CFM_REQ_EVT";
	case ESP_BT_GAP_KEY_NOTIF_EVT: return "KEY_NOTIF_EVT";
	case ESP_BT_GAP_KEY_REQ_EVT: return "KEY_REQ_EVT";
	case ESP_BT_GAP_READ_RSSI_DELTA_EVT: return "READ_RSSI_DELTA_EVT";
	case ESP_BT_GAP_CONFIG_EIR_DATA_EVT: return "CONFIG_EIR_DATA_EVT";
	case ESP_BT_GAP_SET_AFH_CHANNELS_EVT: return "SET_AFH_CHANNELS_EVT";
	case ESP_BT_GAP_READ_REMOTE_NAME_EVT: return "READ_REMOTE_NAME_EVT";
	case ESP_BT_GAP_MODE_CHG_EVT: return "MODE_CHG_EVT";
	case ESP_BT_GAP_REMOVE_BOND_DEV_COMPLETE_EVT: return "REMOVE_BOND_DEV_COMPLETE_EVT";
	case ESP_BT_GAP_QOS_CMPL_EVT: return "QOS_CMPL_EVT";
	case ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT: return "ACL_CONN_CMPL_STAT_EVT";
	case ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT: return "ACL_DISCONN_CMPL_STAT_EVT";
	default: return "UNKNOWN";
	}
	// clang-format on
}