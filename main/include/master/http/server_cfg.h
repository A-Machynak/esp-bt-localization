#pragma once

#include <cstdint>
#include <string_view>

namespace Master
{

/// @brief Available operation modes
enum class WifiOpMode
{
	AP,  ///< Used as Access Point
	STA  ///< Used as a Station
};

/// @brief Wifi configuration for HttpServer
struct WifiConfig
{
	WifiOpMode Mode;                   ///< AP/STA Mode
	std::string_view Ssid;             ///< SSID
	std::string_view Password;         ///< Password
	std::uint8_t ApChannel{1};         ///< Channel used (AP mode)
	std::uint8_t ApMaxConnections{3};  ///< Max allowed connections (AP mode)
};

}  // namespace Master
