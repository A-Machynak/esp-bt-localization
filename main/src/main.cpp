#include <esp_log.h>
#include <nvs_flash.h>
#include <sdkconfig.h>

// clang-format off
#if defined(CONFIG_MASTER)
	#define MAIN_TAG "Master"
	#include "master/master.h"
	static Master::App app;
#elif defined(CONFIG_SCANNER)
	#include "scanner/scanner.h"
	#define MAIN_TAG "Scanner"
	static Scanner::App app;
#else
	#error "'MASTER' or 'SCANNER' isn't defined"
#endif

extern "C" void app_main(void)
{
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	// Config
	#if defined(CONFIG_MASTER)
		Master::AppConfig cfg;
		#if defined(CONFIG_WIFI_AS_AP)
			cfg.WifiCfg = Master::WifiConfig {
				.Mode = Master::WifiOpMode::AP,
				.Ssid = CONFIG_WIFI_SSID,
				.Password = CONFIG_WIFI_PASSWORD,
				.ApChannel = CONFIG_WIFI_CHANNEL,
				.ApMaxConnections = CONFIG_WIFI_MAX_CONNECTIONS,
			};
		#elif defined(CONFIG_WIFI_AS_STA)
			cfg.WifiCfg = Master::WifiConfig {
				.Mode = Master::WifiOpMode::STA,
				.Ssid = CONFIG_WIFI_SSID,
				.Password = CONFIG_WIFI_PASSWORD
			};
		#endif
	#elif defined(CONFIG_SCANNER)
		Scanner::AppConfig cfg;
		#if defined(CONFIG_SCANNER_SCAN_CLASSIC_ONLY)
			cfg.Mode = Scanner::ScanMode::ClassicOnly;
		#elif defined(CONFIG_SCANNER_SCAN_BLE_ONLY)
			cfg.Mode = Scanner::ScanMode::BleOnly;
		#else
			cfg.Mode = Scanner::ScanMode::Both;
			cfg.ScanModePeriod = CONFIG_SCANNER_SCAN_BOTH_PERIOD;
		#endif
	#endif

	app.Init(cfg);
}
// clang-format on
