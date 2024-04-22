#pragma once

#include <cstdint>

namespace Scanner
{

/// @brief Scanning/Discovery mode
enum class ScanMode
{
	ClassicOnly,
	BleOnly,
	Both
};

/// @brief Scanner application configuration
struct AppConfig
{
	/// @brief Scanning mode (Classic/BLE)
	ScanMode Mode{ScanMode::BleOnly};

	/// @brief Scanning periods for each mode [s] (ScanMode::Both).
	std::uint16_t ScanModePeriodClassic{5};
	std::uint16_t ScanModePeriodBle{20};

	/// @brief Interval at which the 'Devices' GATT attribute gets updated.
	std::size_t DevicesUpdateInterval{5'000};

	struct DeviceMemoryConfig
	{
		/// @brief Maximum amount of devices saved in DeviceMemory
		std::size_t MemorySizeLimit{21};

		/// @brief Will merge BLE devices with random BDAs, that use the same advertising data,
		/// into one device with new BDA. At that point, it's assumed that the device changed its
		/// address. This is to reduce the number of detected devices at the cost
		/// of risking merging 2 different devices.
		bool EnableAssociation{false};

		/// @brief Maximum interval in milliseconds, after which a device is removed if not updated
		std::size_t StaleLimit{30'000};

		/// @brief Minimum measured RSSI to save a device.
		std::int8_t MinRssi{-95};
	} DeviceMemoryCfg;
};
}  // namespace Scanner