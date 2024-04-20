#pragma once

#include "core/bt_common.h"
#include "core/utility/mac.h"
#include "core/wrapper/device.h"
#include "core/wrapper/gap_ble_wrapper.h"
#include "core/wrapper/interface/gap_ble_if.h"

#include "tag/tag_cfg.h"

namespace Tag::Impl
{

/// @brief Tag application
class App final : public Gap::Ble::IGapCallback
{
public:
	App(const AppConfig & cfg);

	/// @brief Initialize with configuration
	/// @param config config
	void Init();

	void GapBleAdvStopCmpl(const Gap::Ble::Type::AdvStopCmpl & p);

private:
	const AppConfig _cfg;

	Gap::Ble::Wrapper _bleGap;  ///< BLE GAP

	void _StartAdvertising();
	esp_ble_adv_channel_t _AdvChannelFromConfig();
};

}  // namespace Tag::Impl
