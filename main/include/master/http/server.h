#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <esp_http_server.h>

namespace
{
constexpr std::string_view GetIndexUri = "/";
constexpr std::string_view GetDevicesUri = "/api/devices";
}  // namespace

namespace Master
{

enum class WifiOpMode
{
	AP,  ///< Used as Access Point
	STA  ///< Used as a Station
};

struct WifiConfig
{
	WifiOpMode Mode;                   ///< AP/STA Mode
	std::string_view Ssid;             ///< SSID
	std::string_view Password;         ///< Password
	std::uint8_t ApChannel{1};         ///< Channel used (AP mode)
	std::uint8_t ApMaxConnections{3};  ///< Max allowed connections (AP mode)
};

class HttpServer
{
public:
	HttpServer();
	~HttpServer();

	void Init(const WifiConfig & config);

	esp_err_t GetDevicesHandler(httpd_req_t * r);
	esp_err_t GetIndexHandler(httpd_req_t * r);

	void WifiHandler(esp_event_base_t eventBase, std::int32_t eventId, void * eventData);

	void SetDeviceData(std::span<const char> data);

private:
	std::vector<char> _rawData;

	/// HTTPd server handle
	httpd_handle_t _handle{nullptr};

	/// Operation mode
	WifiOpMode _mode;

	void _InitWifi(const WifiConfig & config);
	void _InitHttp();
	void _InitWifiCommon();
};

}  // namespace Master
