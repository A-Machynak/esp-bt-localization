#pragma once

#include "core/utility/mac.h"
#include "core/wrapper/device.h"

#include <esp_gatt_defs.h>

#include <array>
#include <cstdint>
#include <map>
#include <set>

namespace Master
{

/// @brief Single scanner
struct ScannerInfo
{
	/// @brief Handles for scanner service
	struct ServiceInfo
	{
		std::uint16_t StartHandle{ESP_GATT_INVALID_HANDLE};  ///< Service start handle
		std::uint16_t EndHandle{ESP_GATT_INVALID_HANDLE};    ///< Service end handle
		std::uint16_t StateChar{ESP_GATT_INVALID_HANDLE};    ///< State characteristic handle
		std::uint16_t DevicesChar{ESP_GATT_INVALID_HANDLE};  ///< 'Devices' characteristic handle
	};

	std::uint16_t ConnId;  ///< Connection id
	Mac Bda;               ///< Bluetooth device address
	ServiceInfo Service;   ///< Service info
};

/// @brief Output device data
struct DeviceOut
{
	enum class FlagMask : std::uint8_t
	{
		IsScanner = 0b0000'0001,
		IsBle = 0b0000'0010,
		IsAddrTypePublic = 0b0000'0100
	};
	std::array<std::uint8_t, 6> Bda;  ///< Bluetooth device address
	std::array<float, 3> Position;    ///< The (X,Y,Z) position
	FlagMask Flags;                   ///< Flags. @ref FlagsMask

	constexpr static std::size_t Size = 19;

	void Serialize(std::span<std::uint8_t> output) const;

	static void Serialize(std::span<std::uint8_t> output,
	                      std::span<const std::uint8_t, 6> bda,
	                      std::span<const float, 3> position,
	                      const FlagMask flags);

	static void Serialize(std::span<std::uint8_t> output,
	                      std::span<const std::uint8_t, 6> bda,
	                      std::span<const float, 3> position,
	                      const bool isScanner,
	                      const bool isBle,
	                      const bool isPublic);
};

}  // namespace Master
