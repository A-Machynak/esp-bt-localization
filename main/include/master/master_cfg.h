#pragma once

#include "master/http/server_cfg.h"

#include <cstddef>

namespace Master
{

/// @brief Master application configuration
struct AppConfig
{
	/// @brief Time to wait before sending GATT Reads to each Scanner
	std::size_t GattReadInterval{1000};

	/// @brief Time to wait between each GATT Read
	std::size_t DelayBetweenGattReads{500};

	/// @brief DeviceMemory configuration
	struct DeviceMemoryConfig
	{
		/// @brief Minimum Scanner measurements to calculate a position.
		std::uint8_t MinMeasurements{2};

		/// @brief Minimum connected Scanners to calculate positions.
		std::uint8_t MinScanners{3};

		/// @brief Maximum connected Scanners.
		std::uint8_t MaxScanners{8};

		/// @brief How long before a device gets removed if it doesn't receive a measurement. [ms]
		std::size_t DeviceStoreTime{60'000};

		/// @brief Default path loss at 1m distance used for all devices.
		std::int8_t DefaultPathLoss{45};

		/// @brief Default environment factor used for all devices.
		float DefaultEnvFactor{4.0};

		/// @brief If true, no position calculation/approximation will be done on the Master device.
		/// Instead, it's expected that the data is pulled from the HTTP API Measurements endpoint
		/// and positions calculated somewhere else.
		bool NoPositionCalculation{false};
	} DeviceMemoryCfg;

	/// @brief WiFi configuration
	WifiConfig WifiCfg;
};

}  // namespace Master
