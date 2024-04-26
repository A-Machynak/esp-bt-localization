#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <variant>

#include "esp_eap_client.h"

namespace Master
{

/// @brief Available operation modes
enum class WifiOpMode
{
	AP,  ///< Used as Access Point
	STA  ///< Used as a Station
};

/// @brief WiFi AP config
struct ApConfig
{
	std::string Ssid;                ///< SSID
	std::string Password;            ///< Password
	std::uint8_t Channel{1};         ///< Channel used
	std::uint8_t MaxConnections{3};  ///< Max allowed simultaneous connections
};

enum class EapMethod
{
	Tls,
	Peap,
	Ttls
};

/// @brief WiFi STA config
struct StaConfig
{
	std::string Ssid;      ///< SSID
	std::string Password;  ///< Password

	/// @brief How many times to try to connect to a station before switching to AP mode.
	/// Set to 0 if it should try reconnecting forever.
	std::size_t MaxRetryCount{0};

	bool UseWpa2Enterprise{false};   ///< Use WPA2 Enterprise (EAP) (requires certificate/key).
	bool ValidateWpa2Server{false};  ///< (WPA2) Validate server cert with CA cert.

	/// @brief (WPA2) PEM,CRT,KEY (pointers)
	/// @{
	std::span<std::uint8_t> CaPem;
	std::span<std::uint8_t> ClientCrt;
	std::span<std::uint8_t> ClientKey;
	/// @}

	EapMethod EapMethod_;                 ///< (WPA2) EAP Method
	esp_eap_ttls_phase2_types Phase2Eap;  ///< (WPA2) Phase 2 method for TTLS
	std::string EapId{""};                ///< (WPA2) EAP ID for phase 1
	std::string EapUsername{""};          ///< (WPA2) EAP Username
	std::string EapPassword{""};          ///< (WPA2) EAP Password
};

using ConfigVariant = std::variant<ApConfig, StaConfig>;

/// @brief Wifi configuration for HttpServer
struct WifiConfig
{
	WifiOpMode Mode;  ///< Currently selected mode
	ApConfig Ap;      ///< AP configuration
	StaConfig Sta;    ///< STA configuration
};

}  // namespace Master
