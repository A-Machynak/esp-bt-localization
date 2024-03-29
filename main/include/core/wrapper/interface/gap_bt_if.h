#pragma once

#include <esp_gap_bt_api.h>

namespace Gap::Bt
{

namespace Type
{

using DiscRes = esp_bt_gap_cb_param_t::disc_res_param;
using DiscStateChanged = esp_bt_gap_cb_param_t::disc_state_changed_param;
using RmtSrvcs = esp_bt_gap_cb_param_t::rmt_srvcs_param;
using RmtSrvcRec = esp_bt_gap_cb_param_t::rmt_srvc_rec_param;
using AuthCmpl = esp_bt_gap_cb_param_t::auth_cmpl_param;
using PinReq = esp_bt_gap_cb_param_t::pin_req_param;
using CfmReq = esp_bt_gap_cb_param_t::cfm_req_param;
using KeyNotif = esp_bt_gap_cb_param_t::key_notif_param;
using KeyReq = esp_bt_gap_cb_param_t::key_req_param;
using ReadRssiDelta = esp_bt_gap_cb_param_t::read_rssi_delta_param;
using ConfigEirData = esp_bt_gap_cb_param_t::config_eir_data_param;
using SetAfhChannels = esp_bt_gap_cb_param_t::set_afh_channels_param;
using ReadRemoteName = esp_bt_gap_cb_param_t::read_rmt_name_param;
using ModeChg = esp_bt_gap_cb_param_t::mode_chg_param;
using RemoveBondDevCmpl = esp_bt_gap_cb_param_t::bt_remove_bond_dev_cmpl_evt_param;
using QosCmpl = esp_bt_gap_cb_param_t::qos_cmpl_param;
using AclConnCmplStat = esp_bt_gap_cb_param_t::acl_conn_cmpl_stat_param;
using AclDisconnCmplStat = esp_bt_gap_cb_param_t::acl_disconn_cmpl_stat_param;

}  // namespace Type

using namespace Type;

/// @brief BT GAP callback interface
class IGapCallback
{
public:
	virtual ~IGapCallback() {}

	// clang-format off
	virtual void GapBtDiscRes(const DiscRes &) {}                ///< ESP_BT_GAP_DISC_RES_EVT
	virtual void GapBtDiscStateChanged(const DiscStateChanged &) {}  ///< ESP_BT_GAP_DISC_STATE_CHANGED_EVT
	virtual void GapBtRmtSrvcs(const RmtSrvcs &) {}              ///< ESP_BT_GAP_RMT_SRVCS_EVT
	virtual void GapBtRmtSrvcRec(const RmtSrvcRec &) {}          ///< ESP_BT_GAP_RMT_SRVC_REC_EVT
	virtual void GapBtAuthCmpl(const AuthCmpl &) {}              ///< ESP_BT_GAP_AUTH_CMPL_EVT
	virtual void GapBtPinReq(const PinReq &) {}                  ///< ESP_BT_GAP_PIN_REQ_EVT
	virtual void GapBtCfmReq(const CfmReq &) {}                  ///< ESP_BT_GAP_CFM_REQ_EVT
	virtual void GapBtKeyNotif(const KeyNotif &) {}              ///< ESP_BT_GAP_KEY_NOTIF_EVT
	virtual void GapBtKeyReq(const KeyReq &) {}                  ///< ESP_BT_GAP_KEY_REQ_EVT
	virtual void GapBtReadRssiDelta(const ReadRssiDelta &) {}    ///< ESP_BT_GAP_READ_RSSI_DELTA_EVT
	virtual void GapBtConfigEirData(const ConfigEirData &) {}    ///< ESP_BT_GAP_CONFIG_EIR_DATA_EVT
	virtual void GapBtSetAfhChannels(const SetAfhChannels &) {}  ///< ESP_BT_GAP_SET_AFH_CHANNELS_EVT
	virtual void GapBtReadRemoteName(const ReadRemoteName &) {}  ///< ESP_BT_GAP_READ_REMOTE_NAME_EVT
	virtual void GapBtModeChg(const ModeChg &) {}                ///< ESP_BT_GAP_MODE_CHG_EVT
	virtual void GapBtRemoveBondDevCmpl(const RemoveBondDevCmpl &) {}  ///< ESP_BT_GAP_REMOVE_BOND_DEV_COMPLETE_EVT
	virtual void GapBtQosCmpl(const QosCmpl &) {}                ///< ESP_BT_GAP_QOS_CMPL_EVT
	virtual void GapBtAclConnCmplStat(const AclConnCmplStat &) {}  ///< ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT
	virtual void GapBtAclDisconnCmplStat(const AclDisconnCmplStat &) {}  ///< ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT
	// clang-format on
};

}  // namespace Gap::Bt
