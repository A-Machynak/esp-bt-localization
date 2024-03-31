#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <esp_http_server.h>

#include "master/http/server_cfg.h"

namespace
{
/// @brief Index page URI
constexpr std::string_view GetIndexUri = "/";

/// @brief Device API URI
constexpr std::string_view GetDevicesUri = "/api/devices";
}  // namespace

namespace Master::Impl
{

/// @brief Basic HTTP server implementation for Master device with 2 endpoints
/// (GetIndexUri, GetDevicesUri).
class HttpServer final
{
public:
	/// @brief Constructor, initialize with Init (in case of static initialization)
	HttpServer();
	~HttpServer();

	/// @brief Initialize with configuration
	/// @param config config
	void Init(const WifiConfig & config);

	/// @brief URI handlers callbacks. Not meant to be called directly.
	/// @param r request
	/// @return OK; otherwise disconnect
	/// @{
	esp_err_t GetDevicesHandler(httpd_req_t * r);
	esp_err_t GetIndexHandler(httpd_req_t * r);
	/// @}

	/// @brief Wifi handler callback. Not meant to be called directly.
	/// @param eventBase event
	/// @param eventId event id
	/// @param eventData data
	void WifiHandler(esp_event_base_t eventBase, std::int32_t eventId, void * eventData);

	/// @brief Set data returned from API endpoint
	/// @param data raw data
	void SetRawData(std::span<const char> data);

private:
	/// HTTPd server handle
	httpd_handle_t _handle{nullptr};
	/// Operation mode
	WifiOpMode _mode;
	/// Data for API endpoint
	std::vector<char> _rawData;

	/// @brief Initializers
	/// @{
	void _InitWifi(const WifiConfig & config);
	void _InitHttp();
	void _InitWifiCommon();
	/// @}
};

}  // namespace Master::Impl
