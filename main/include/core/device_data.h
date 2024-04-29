#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <span>

#include <esp_gap_ble_api.h>

namespace Core
{
using Clock = std::chrono::system_clock;
using TimePoint = std::chrono::time_point<Clock>;

/// @brief Maximum amount of devices
constexpr std::size_t DefaultMaxDevices = 128;

/// @brief Mask for flags
enum FlagMask : std::uint8_t
{
	IsBle = 0b0000'0001,
	IsAddrTypePublic = 0b0000'0010,
};

/// @brief View into device data.
/// Only provides a better interface to access elements and add new ones in the future.
/// We are assuming that basically everything here gets inlined.
///
/// This is the data sent from a Scanner to the Master.
struct DeviceDataView
{
	/// @brief Data size.
	/// - Timepoint (UNIX timestamp) - 4B
	/// - MAC - 6B
	/// - RSSI - 1B
	/// - Flags - 1B
	/// - Advertising data length - 1B
	/// - Advertising event type - 1B (ignored for BR/EDR)
	/// - Advertising data - 62B
	constexpr static std::size_t Size = 76;

	/// @brief Indices for `Data`
	/// @{
	constexpr static std::size_t TimepointIdx = 0;
	constexpr static std::size_t MacStartIdx = 4;
	constexpr static std::size_t MacEndIdx = MacStartIdx + 5;
	constexpr static std::size_t RssiIdx = MacEndIdx + 1;
	constexpr static std::size_t FlagsIdx = RssiIdx + 1;
	constexpr static std::size_t AdvDataSizeIdx = FlagsIdx + 1;
	constexpr static std::size_t AdvEventTypeIdx = AdvDataSizeIdx + 1;
	constexpr static std::size_t AdvDataStartIdx = AdvEventTypeIdx + 1;
	constexpr static std::size_t AdvDataEndIdx = AdvDataStartIdx + 61;
	/// @}

	/// @brief Constructor from raw data
	/// @param data raw data
	DeviceDataView(std::span<std::uint8_t, DeviceDataView::Size> data);

	/// @brief UNIX timestamp (seconds)
	/// @return Timepoint data
	/// @{
	std::uint32_t & Timestamp();
	std::uint32_t Timestamp() const;
	/// @}

	/// @brief MAC data
	/// @return MAC data
	/// @{
	std::span<std::uint8_t, 6> Mac();
	std::span<const std::uint8_t, 6> Mac() const;
	/// @}

	/// @brief RSSI data
	/// @return RSSI data
	/// @{
	std::int8_t & Rssi();
	std::int8_t Rssi() const;
	/// @}

	/// @brief Flags
	/// @return Flags data
	/// @{
	std::uint8_t & Flags();
	std::uint8_t Flags() const;
	/// @}

	/// @brief Advertising data size
	/// @return Advertising data size
	/// @{
	std::uint8_t & AdvDataSize();
	std::uint8_t AdvDataSize() const;
	/// @}

	/// @brief Advertising event type
	/// @return Advertising event type
	/// @{
	esp_ble_evt_type_t & EventType();
	esp_ble_evt_type_t EventType() const;
	/// @}

	/// @brief Advertising data
	/// This could be advertising response data/scan response data/EIR for BLE
	/// or custom data for BR/EDR (TODO)
	/// @return Advertising data span
	/// @{
	std::span<std::uint8_t, 62> AdvData();
	std::span<const std::uint8_t, 62> AdvData() const;
	/// @}

	/// @brief Is address type public (BLE)
	/// @return true/false
	bool IsAddrTypePublic() const;

	/// @brief Is BLE device
	/// @return true/false
	bool IsBle() const;

	/// @brief Data span
	std::span<std::uint8_t, Size> Span;

	/// @brief Array of DeviceDataView for convenience.
	struct Array
	{
		/// @brief Constructor from raw data
		/// @param data raw data; expected to be divisible by DeviceDataView::Size
		Array(std::span<std::uint8_t> data);

		/// @brief Access view at index; no checks are provided.
		/// @param i index
		/// @return view
		/// @{
		DeviceDataView operator[](std::size_t i);
		DeviceDataView operator[](std::size_t i) const;
		/// @}

		/// @brief Data view
		std::span<std::uint8_t> Span;

		/// @brief How many views does the array contain
		std::size_t Size;
	};
};

/// @brief Storage for device data with its view
struct DeviceData
{
	/// @brief Constructor from raw data
	/// @param data raw data; takes a maximum of 72 bytes
	DeviceData(std::span<const std::uint8_t> data);

	/// @brief Constructor
	/// @param timestamp UNIX timestamp (seconds)
	/// @param mac BDA
	/// @param rssi RSSI
	/// @param flags Flags @ref FlagMask
	/// @param eventType BLE event type
	/// @param advData advertising data
	DeviceData(std::uint32_t timestamp,
	           std::span<const std::uint8_t, 6> mac,
	           std::int8_t rssi,
	           FlagMask flags,
	           esp_ble_evt_type_t eventType,
	           std::span<const std::uint8_t> advData);

	/// @brief Packed raw data in array
	std::array<std::uint8_t, DeviceDataView::Size> Data;

	/// @brief Data view to access it
	DeviceDataView View;
};

/// @brief Calculate memory used by devices when serialized
/// @param devices how many devices
/// @return how many bytes of memory will be used
constexpr std::size_t DeviceMemoryByteSize(std::size_t devices = DefaultMaxDevices)
{
	return devices * Core::DeviceDataView::Size;
}

}  // namespace Core
