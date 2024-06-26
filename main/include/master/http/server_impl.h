#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string_view>
#include <vector>

#include <esp_http_server.h>
#include <esp_netif.h>

#include "master/http/server_cfg.h"

namespace
{
/// @brief Index page URI
constexpr std::string_view IndexUri = "/";

/// @brief Device API URI
constexpr std::string_view DevicesUri = "/api/devices";

/// @brief Config API URI
constexpr std::string_view ConfigUri = "/api/config";

/// @brief Maximum length for API POST data
constexpr std::size_t PostDevicesLengthLimit = 64;
}  // namespace

namespace Master::Impl
{

/// @brief Basic HTTP server implementation for Master device with 2 endpoints
/// (IndexUri, DevicesUri).
class HttpServer final
{
public:
	/// @brief Constructor, initialize with Init (in case of static initialization)
	HttpServer(const WifiConfig & cfg);
	~HttpServer();

	/// @brief Initialize
	void Init();

	/// @brief URI handlers callbacks. Not meant to be called directly.
	/// @param r request
	/// @return OK; otherwise disconnect
	/// @{
	esp_err_t GetIndexHandler(httpd_req_t * r);
	esp_err_t GetDevicesHandler(httpd_req_t * r);
	esp_err_t PostConfigHandler(httpd_req_t * r);
	/// @}

	/// @brief Wifi handler callback. Not meant to be called directly.
	/// @param eventBase event
	/// @param eventId event id
	/// @param eventData data
	void WifiHandler(esp_event_base_t eventBase, std::int32_t eventId, void * eventData);

	/// @brief Set the data for GET DevicesUri
	/// @param data raw data
	void SetDevicesGetData(std::span<const char> data);

	/// @brief Listener for POST ConfigUri
	/// @param fn function
	void SetConfigPostListener(std::function<void(std::span<const char>)> fn);

	/// @brief Switch to another WiFi mode.
	/// @param mode Wifi mode
	void SwitchMode(WifiOpMode mode);

	/// @brief Restart server
	void Restart();

private:
	/// Configuration
	WifiConfig _cfg;
	/// HTTPd server handle
	httpd_handle_t _handle{nullptr};
	/// Data for API endpoint
	std::vector<char> _rawData;
	/// Function called for API endpoint POST request
	std::function<void(std::span<const char>)> _postConfigListener;

	/// Network interface
	esp_netif_t * _netIf{nullptr};

	/// @brief Initializers
	/// @{
	void _InitNetIfWifi();
	void _InitWifi();
	void _InitWifiMode();
	void _InitHttp();
	/// @}
};

}  // namespace Master::Impl
