#pragma once

#include <cstdint>
#include <span>

#include "core/clock.h"
#include "core/device_data.h"

namespace Scanner
{
/// @brief Device info
class DeviceInfo
{
public:
	/// @brief Constructor
	/// @param bda Bluetooth device address
	/// @param rssi RSSI
	/// @param flags flags
	/// @param eventType BLE event type
	/// @param data advertising data
	DeviceInfo(std::span<const std::uint8_t, 6> bda,
	           std::int8_t rssi,
	           Core::FlagMask flags,
	           esp_ble_evt_type_t eventType,
	           std::span<const std::uint8_t> data);

	/// @brief Update RSSI value
	/// @param rssi RSSI
	void Update(std::int8_t rssi);

	/// @brief Update RSSI and MAC (for devices with random BDA)
	/// @param bda Bluetooth device address
	/// @param rssi RSSI
	void Update(std::span<const std::uint8_t, 6> bda, std::int8_t rssi);

	/// @brief Serialize
	/// @param[out] output output destination
	void Serialize(std::span<std::uint8_t, Core::DeviceDataView::Size> output) const;

	/// @brief Data getter
	/// @return device data
	const Core::DeviceData & GetDeviceData() const;

	/// @brief Last update time point
	/// @return time point
	const Core::TimePoint & GetLastUpdate() const;

private:
	/// @brief Average of the last `Size` values
	class AverageWindow
	{
	public:
		AverageWindow(std::int8_t value);

		void Add(std::int8_t value);

		std::int8_t Average() const;

	public:
		constexpr static std::uint8_t Size = 10;
		std::array<std::int8_t, Size> Window;
		std::uint8_t Idx{0};

		static std::uint8_t _NextIdx(std::uint8_t idx);
	};

	Core::DeviceData _outData;     ///< Raw output data
	Core::TimePoint _firstUpdate;  ///< First update
	Core::TimePoint _lastUpdate;   ///< Last RSSI update
	AverageWindow _rssi;           ///< Average window for rssi
};

}  // namespace Scanner
