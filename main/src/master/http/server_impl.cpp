#include "master/http/server_impl.h"

#include "core/utility/mac.h"
#include "core/wrapper/wifi.h"
#include "master/http/index_page.h"

#include <algorithm>
#include <functional>

#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>

#include <esp_log.h>

namespace
{
static const char * TAG = "HttpServer";

/// @brief RAII AP configuration
static wifi_config_t ApConfig(std::string_view ssid,
                              std::string_view password,
                              std::uint8_t channel,
                              std::uint8_t maxConnections)
{
	const std::size_t ssidSize = std::clamp((int)ssid.size(), 0, 31);
	const std::size_t passSize = std::clamp((int)password.size(), 0, 63);

	wifi_config_t cfg;
	std::copy_n(ssid.begin(), ssidSize, cfg.ap.ssid);
	cfg.ap.ssid[ssidSize] = '\0';

	if (password.size() < 8) {
		if (password.size() != 0) {
			ESP_LOGW(TAG, "Password length < 8. Setting authentication mode to OPEN");
		}
		cfg.ap.authmode = wifi_auth_mode_t::WIFI_AUTH_OPEN;
	}
	else {
		std::copy_n(password.begin(), passSize, cfg.ap.password);
		cfg.ap.password[passSize] = '\0';

		cfg.ap.authmode = wifi_auth_mode_t::WIFI_AUTH_WPA2_PSK;
	}

	cfg.ap.ssid_len = ssid.size();
	cfg.ap.channel = channel;
	cfg.ap.ssid_hidden = 0;
	cfg.ap.max_connection = maxConnections;
	cfg.ap.beacon_interval = 10000;
	// cfg.ap.pairwise_cipher
	cfg.ap.ftm_responder = false;
	// cfg.ap.pmf_cfg
	// cfg.ap.sae_pwe_h2e

	return cfg;
}

/// @brief RAII STA configuration
static wifi_config_t StaConfig(std::string_view ssid, std::string_view password)
{
	const std::size_t ssidSize = std::clamp((int)ssid.size(), 0, 31);
	const std::size_t passSize = std::clamp((int)password.size(), 0, 63);

	wifi_config_t cfg;
	std::copy_n(ssid.begin(), ssidSize, cfg.sta.ssid);
	cfg.sta.ssid[ssidSize] = '\0';
	std::copy_n(password.begin(), passSize, cfg.sta.password);
	cfg.sta.password[passSize] = '\0';
	return cfg;
}

void WifiHandlerPassthrough(void * arg,
                            esp_event_base_t eventBase,
                            std::int32_t eventId,
                            void * eventData)
{
	reinterpret_cast<Master::Impl::HttpServer *>(arg)->WifiHandler(eventBase, eventId, eventData);
}

/// generic passthrough for URI handlers; even I am a bit disgusted
template <esp_err_t (Master::Impl::HttpServer::*Fn)(httpd_req_t *)>
esp_err_t HandlerPassthrough(httpd_req_t * r)
{
	return (reinterpret_cast<Master::Impl::HttpServer *>(r->user_ctx)->*Fn)(r);
}
}  // namespace

namespace Master::Impl
{

HttpServer::HttpServer() {}

HttpServer::~HttpServer()
{
	httpd_stop(_handle);
}

void HttpServer::Init(const WifiConfig & config)
{
	_mode = config.Mode;
	_InitWifi(config);
	_InitHttp();
}

esp_err_t HttpServer::GetIndexHandler(httpd_req_t * r)
{
	httpd_resp_set_type(r, "text/html");
	httpd_resp_set_hdr(r, "Content-Encoding", "gzip");
	httpd_resp_send(r, IndexPageGzip.data(), IndexPageGzip.size());
	return ESP_OK;
}

esp_err_t HttpServer::GetDevicesHandler(httpd_req_t * r)
{
	httpd_resp_set_type(r, "text/plain");
	httpd_resp_send(r, _rawData.data(), _rawData.size());
	return ESP_OK;
}

esp_err_t HttpServer::PostConfigHandler(httpd_req_t * r)
{
	if (r->content_len > PostDevicesLengthLimit) {
		httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Content length too big");
		return ESP_FAIL;
	}

	static std::array<char, PostDevicesLengthLimit> data;

	std::size_t read = 0;
	while (read < r->content_len) {
		int singleRead = httpd_req_recv(r, data.data() + read, r->content_len);
		if (singleRead <= 0) {
			httpd_resp_send_err(r, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed reading post data");
			return ESP_FAIL;
		}
		read += singleRead;
	}
	if (_postConfigListener) {
		_postConfigListener(std::span<const char>(data.data(), r->content_len));
	}
	httpd_resp_sendstr(r, "OK");
	return ESP_OK;
}

void HttpServer::SetDevicesGetData(std::span<const char> data)
{
	_rawData.resize(data.size());
	ESP_LOGD(TAG, "Setting data, length %d", data.size());
	std::copy(data.begin(), data.end(), _rawData.data());
}

void HttpServer::SetConfigPostListener(std::function<void(std::span<const char>)> fn)
{
	_postConfigListener = fn;
}

void HttpServer::WifiHandler(esp_event_base_t eventBase, std::int32_t eventId, void * eventData)
{
	const wifi_event_t event = static_cast<wifi_event_t>(eventId);

	if (_mode == WifiOpMode::STA) {
		// STA - connect
		if (eventBase == WIFI_EVENT) {
			if (eventId == WIFI_EVENT_STA_START) {
				esp_wifi_connect();
			}
			else if (eventId == WIFI_EVENT_STA_DISCONNECTED) {
				const wifi_event_sta_disconnected_t * event =
				    (wifi_event_sta_disconnected_t *)eventData;
				ESP_LOGW(TAG, "Station disconnected (%s, %s), retrying...", event->ssid,
				         ToString(static_cast<wifi_err_reason_t>(event->reason)));
				esp_wifi_connect();
			}
		}
		else if (eventBase == IP_EVENT) {
			if (eventId == IP_EVENT_STA_GOT_IP) {
				const ip_event_got_ip_t * e = reinterpret_cast<ip_event_got_ip_t *>(eventData);
				ESP_LOGI(TAG, "Connected, got IP " IPSTR, IP2STR(&e->ip_info.ip));
			}
		}
	}
	else {
		// AP - just log some info
		if (event == WIFI_EVENT_AP_STACONNECTED) {
			const wifi_event_ap_staconnected_t * event = (wifi_event_ap_staconnected_t *)eventData;
			ESP_LOGI(TAG, "%s connected, AID=%d", ToString(event->mac).c_str(), event->aid);
		}
		else if (event == WIFI_EVENT_AP_STADISCONNECTED) {
			const wifi_event_ap_stadisconnected_t * event =
			    (wifi_event_ap_stadisconnected_t *)eventData;
			ESP_LOGI(TAG, "%s disconnected, AID=%d", ToString(event->mac).c_str(), event->aid);
		}
	}
}

void HttpServer::_InitWifi(const WifiConfig & config)
{
	esp_netif_init();
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	if (config.Mode == WifiOpMode::AP) {
		esp_netif_create_default_wifi_ap();
	}
	else {
		esp_netif_create_default_wifi_sta();
	}

	const wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
	                                                    &WifiHandlerPassthrough, this, nullptr));

	if (config.Mode == WifiOpMode::AP) {
		ESP_ERROR_CHECK(esp_wifi_set_mode(wifi_mode_t::WIFI_MODE_AP));

		wifi_config_t cfg =
		    ApConfig(config.Ssid, config.Password, config.ApChannel, config.ApMaxConnections);
		ESP_ERROR_CHECK(esp_wifi_set_config(wifi_interface_t::WIFI_IF_AP, &cfg));
		ESP_LOGI(TAG, "Configured as AP (ssid \"%s\" pw \"%s\")", cfg.ap.ssid, cfg.ap.password);
	}
	else {
		ESP_ERROR_CHECK(esp_event_handler_instance_register(
		    IP_EVENT, IP_EVENT_STA_GOT_IP, &WifiHandlerPassthrough, this, nullptr));

		ESP_ERROR_CHECK(esp_wifi_set_mode(wifi_mode_t::WIFI_MODE_STA));

		wifi_config_t cfg = StaConfig(config.Ssid, config.Password);
		ESP_ERROR_CHECK(esp_wifi_set_config(wifi_interface_t::WIFI_IF_STA, &cfg));
		ESP_LOGI(TAG, "Configured as STA (ssid \"%s\" pw \"%s\")", cfg.sta.ssid, cfg.sta.password);
	}
	ESP_ERROR_CHECK(esp_wifi_start());
}

void HttpServer::_InitHttp()
{
	httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
	cfg.lru_purge_enable = true;
	ESP_ERROR_CHECK(httpd_start(&_handle, &cfg));

	const httpd_uri_t getIndex{
	    .uri = IndexUri.data(),
	    .method = httpd_method_t::HTTP_GET,
	    .handler = &HandlerPassthrough<&HttpServer::GetIndexHandler>,
	    .user_ctx = this,
	};
	const httpd_uri_t getDevices{
	    .uri = DevicesUri.data(),
	    .method = httpd_method_t::HTTP_GET,
	    .handler = &HandlerPassthrough<&HttpServer::GetDevicesHandler>,
	    .user_ctx = this,
	};
	const httpd_uri_t postConfig{
	    .uri = ConfigUri.data(),
	    .method = httpd_method_t::HTTP_POST,
	    .handler = &HandlerPassthrough<&HttpServer::PostConfigHandler>,
	    .user_ctx = this,
	};

	ESP_ERROR_CHECK(httpd_register_uri_handler(_handle, &getIndex));
	ESP_ERROR_CHECK(httpd_register_uri_handler(_handle, &getDevices));
	ESP_ERROR_CHECK(httpd_register_uri_handler(_handle, &postConfig));
}

}  // namespace Master::Impl
