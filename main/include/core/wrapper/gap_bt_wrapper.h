#pragma once

#include "core/wrapper/interface/gap_bt_if.h"
#include <esp_gap_bt_api.h>

#include <limits>

namespace Gap::Bt
{
constexpr float DiscoverForever = std::numeric_limits<float>::max();

/// @brief Scan connection mode
using ConnectionMode = esp_bt_connection_mode_t;

/// @brief Scan discovery mode
using DiscoveryMode = esp_bt_discovery_mode_t;

/// @brief Type of inquiry for scanning
using InquiryMode = esp_bt_inq_mode_t;

#ifdef CONFIG_BT_CLASSIC_ENABLED

/// @brief Wrapper for BT GAP functions to provide more 'C++-like' interface
/// A lot of methods are missing, since they aren't used in this project.
class Wrapper
{
public:
	Wrapper(IGapCallback * callback);
	~Wrapper();

	/// @brief Initialization.
	/// Cannot be called during constructor (can be static).
	void Init();

	/// @brief Scan mode setter
	/// @param connectionMode connection mode
	/// @param discoveryMode discovery mode
	void SetScanMode(ConnectionMode connectionMode, DiscoveryMode discoveryMode);

	/// @brief Start/Stop Discovery
	/// @{
	void StartDiscovery(InquiryMode inquiryMode, float time);
	void StopDiscovery();
	/// @}

	/// @brief Bluetooth GAP callback. Not meant to be called directly.
	/// @param event event
	/// @param param event params
	void BtGapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t * param);

private:
	IGapCallback * _callback;

	float _discoveryTime = 0.0;
	bool _isDiscovering = false;

	void _StartDiscoveryImpl(InquiryMode inquiryMode);
};

#else

/// @brief Dummy wrapper for disabled bluetooth
class Wrapper
{
public:
	Wrapper([[maybe_unused]] IGapCallback * callback);
	void Init() {}
	void SetScanMode(ConnectionMode connectionMode, DiscoveryMode discoveryMode) {}
	void StartDiscovery(InquiryMode inquiryMode, float time) {}
	void StopDiscovery() {}
	void BtGapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t * param) {}
};

#endif
}  // namespace Gap::Bt

const char * ToString(const esp_bt_gap_cb_event_t event);
