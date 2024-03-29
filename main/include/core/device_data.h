#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>

#include <esp_gap_ble_api.h>

namespace Core
{

/// @brief Mask for flags
enum FlagMask : std::uint8_t
{
	IsBle = 0b0000'0001,
	IsAddrTypePublic = 0b0000'0010,
};

/// @brief View into device data.
/// Only provides a better interface to access elements and add new ones in the future.
/// This is the data sent from a Scanner to the Master.
struct DeviceDataView
{
	/// @brief Data size.
	/// - MAC - 6B
	/// - RSSI - 1B
	/// - Flags - 1B
	/// - Advertising data length - 1B
	/// - Advertising event type - 1B (ignored for BR/EDR)
	/// - Advertising data - 62B
	constexpr static std::size_t Size = 72;

	/// @brief Indices for `Data`
	/// @{
	constexpr static std::size_t MacStartIdx = 0;
	constexpr static std::size_t MacEndIdx = 5;
	constexpr static std::size_t RssiIdx = 6;
	constexpr static std::size_t FlagsIdx = 7;
	constexpr static std::size_t AdvDataSizeIdx = 8;
	constexpr static std::size_t AdvEventTypeIdx = 9;
	constexpr static std::size_t AdvDataStartIdx = 10;
	constexpr static std::size_t AdvDataEndIdx = 71;
	/// @}

	/// @brief Constructor from raw data
	/// @param data raw data; takes a maximum of 72 bytes
	DeviceDataView(std::span<std::uint8_t, DeviceDataView::Size> data);

	/// @brief MAC data; bytes 0-5
	/// @return MAC data
	/// @{
	std::span<std::uint8_t, 6> Mac();
	std::span<const std::uint8_t, 6> Mac() const;
	/// @}

	/// @brief RSSI data; byte 6
	/// @return RSSI data
	/// @{
	std::int8_t & Rssi();
	std::int8_t Rssi() const;
	/// @}

	/// @brief Flags; byte 7
	/// @return Flags data
	/// @{
	std::uint8_t & Flags();
	std::uint8_t Flags() const;
	/// @}

	/// @brief Advertising data size; byte 8
	/// @return Advertising data size
	/// @{
	std::uint8_t & AdvDataSize();
	std::uint8_t AdvDataSize() const;
	/// @}

	/// @brief Advertising event type; byte 9
	/// @return Advertising event type
	/// @{
	esp_ble_evt_type_t & EventType();
	esp_ble_evt_type_t EventType() const;
	/// @}

	/// @brief Advertising data; bytes 10 - 71
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
};

/// @brief Storage for device data with its view
struct DeviceData
{
	/// @brief Constructor from raw data
	/// @param data raw data; takes a maximum of 72 bytes
	DeviceData(std::span<const std::uint8_t> data);

	/// @brief Constructor
	/// @param mac BDA
	/// @param rssi RSSI
	/// @param flags Flags @ref FlagMask
	/// @param eventType BLE event type
	/// @param advData advertising data
	DeviceData(std::span<const std::uint8_t, 6> mac,
	           std::int8_t rssi,
	           std::uint8_t flags,
	           esp_ble_evt_type_t eventType,
	           std::span<const std::uint8_t> advData);

	/// @brief Packed raw data in array
	std::array<std::uint8_t, DeviceDataView::Size> Data;

	/// @brief Data view to access it
	DeviceDataView View;
};

}  // namespace Core
