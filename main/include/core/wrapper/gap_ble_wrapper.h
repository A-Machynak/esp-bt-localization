#pragma once

#include <esp_gap_ble_api.h>

#include "core/wrapper/interface/gap_ble_if.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <span>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>

namespace Gap::Ble
{
constexpr float AdvertiseForever = -1.0;
constexpr float ScanForever = std::numeric_limits<float>::max();

/// @brief Wrapper for BLE GAP functions to provide more 'C++-like' interface
/// A lot of methods are missing, since they aren't used in this project.
class Wrapper
{
public:
	Wrapper(IGapCallback * callback);
	~Wrapper();

	/// @brief Mandatory initialization.
	/// Cannot be called during constructor (can be static).
	void Init();

	/// @brief Start/Stop advertising
	/// @{
	void StartAdvertising(esp_ble_adv_params_t * params, float time = AdvertiseForever);
	void StopAdvertising();
	/// @}

	/// @brief Start/Stop scanning
	/// @{
	void StartScanning(float time = ScanForever);
	void StopScanning();
	/// @}

	/// @brief Setters
	/// @{
	void SetScanParams(esp_ble_scan_params_t * params);
	void SetRawAdvertisingData(std::span<std::uint8_t> data);
	/// @}

	/// @brief BLE GAP Callback. Not meant to be called directly.
	/// @param event event
	/// @param param parameters
	void BleGapCallback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t * param);

private:
	IGapCallback * _callback = nullptr;

	/// @brief Timers for advertising
	/// @{
	TimerHandle_t _advTimerHandle = nullptr;
	StaticTimer_t _advTimerBuffer;
	/// @}

	/// @brief Used for scanning forever
	/// @{
	bool _scanForever = false;
	bool _firstScanMessage = true;
	/// @}

	bool _isScanning = false;
	bool _isAdvertising = false;
};

/// @brief Conversion from seconds to uint16 representation for the purpose of specifying
/// advertising interval
/// @param seconds seconds in the range <0.02, 10.24>
/// @return seconds converted with the formula [Time = N * 0.625 msec]
constexpr std::uint16_t ConvertAdvertisingInterval(float seconds);

/// @brief Conversion from seconds to uint16 representation for the purpose of specifying
/// scan interval or scan window (same formula and range)
/// @param seconds seconds in the range <0.0025, 10.24>
/// @return seconds converted with the formula [Time = N * 0.625 msec]
constexpr std::uint16_t ConvertScanInterval(float seconds);

}  // namespace Gap::Ble

const char * ToString(esp_gap_ble_cb_event_t event);

#include "core/wrapper/gap_ble_wrapper.hpp"
