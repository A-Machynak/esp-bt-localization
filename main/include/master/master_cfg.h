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

	/// @brief WiFi configuration
	WifiConfig WifiCfg;
};

}  // namespace Master
