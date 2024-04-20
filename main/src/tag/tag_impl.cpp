#include "tag/tag_impl.h"

#include <esp_log.h>

namespace
{
/// @brief Logger tag
// static const char * TAG = "Tag";
}  // namespace

namespace Tag::Impl
{
App::App(const AppConfig & cfg)
    : _cfg(cfg)
    , _bleGap(this)
{
}

void App::Init()
{
	Bt::EnableBtController();
	Bt::EnableBluedroid();

	_bleGap.Init();

	_StartAdvertising();
}

void App::GapBleAdvStopCmpl(const Gap::Ble::Type::AdvStopCmpl & p)
{
	_StartAdvertising();
}

void App::_StartAdvertising()
{
	esp_ble_adv_params_t advParams{
	    .adv_int_min = _cfg.AdvIntMin,
	    .adv_int_max = _cfg.AdvIntMax,
	    .adv_type = esp_ble_adv_type_t::ADV_TYPE_IND,
	    .own_addr_type = esp_ble_addr_type_t::BLE_ADDR_TYPE_PUBLIC,
	    .peer_addr = {0},
	    .peer_addr_type = esp_ble_addr_type_t::BLE_ADDR_TYPE_PUBLIC,
	    .channel_map = _AdvChannelFromConfig(),
	    .adv_filter_policy = esp_ble_adv_filter_t::ADV_FILTER_ALLOW_SCAN_WLST_CON_WLST,
	};
	_bleGap.StartAdvertising(&advParams);
}

esp_ble_adv_channel_t App::_AdvChannelFromConfig()
{
	if (_cfg.AdvertiseOnAllChannels) {
		return esp_ble_adv_channel_t::ADV_CHNL_ALL;
	}

	switch (_cfg.ChannelToAdvertiseOn) {
	case 37:
		return esp_ble_adv_channel_t::ADV_CHNL_37;
	case 38:
		return esp_ble_adv_channel_t::ADV_CHNL_38;
	case 39:
		return esp_ble_adv_channel_t::ADV_CHNL_39;
	default:
		return esp_ble_adv_channel_t::ADV_CHNL_ALL;
	}
}
}  // namespace Tag::Impl
