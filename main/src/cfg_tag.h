#pragma once

#include "tag/tag_cfg.h"
#include <sdkconfig.h>

// clang-format off
constexpr Tag::AppConfig Cfg
{
	.AdvIntMin = CONFIG_TAG_MINIMUM_ADVERTISING_INTERVAL,
	.AdvIntMax = CONFIG_TAG_MAXIMUM_ADVERTISING_INTERVAL,
#if defined(CONFIG_TAG_ADVERTISING_CHANNEL_ALL)
	.AdvertiseOnAllChannels = true,
	.ChannelToAdvertiseOn = {},
#else
	.AdvertiseOnAllChannels = false,
	.ChannelToAdvertiseOn = CONFIG_TAG_ADVERTISING_CHANNEL,
#endif
};
// clang-format on
