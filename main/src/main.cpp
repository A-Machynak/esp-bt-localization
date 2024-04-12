#include <esp_log.h>
#include <nvs_flash.h>
#include <sdkconfig.h>

// clang-format off
#if defined(CONFIG_MASTER)
    #define MAIN_TAG "Master"
    #include "master/master.h"

    Master::AppConfig cfg {
    #if defined(CONFIG_WIFI_AS_AP)
        .WifiCfg = Master::WifiConfig {
            .Mode = Master::WifiOpMode::AP,
            .Ssid = CONFIG_WIFI_SSID,
            .Password = CONFIG_WIFI_PASSWORD,
            .ApChannel = CONFIG_WIFI_CHANNEL,
            .ApMaxConnections = CONFIG_WIFI_MAX_CONNECTIONS,
        },
    #elif defined(CONFIG_WIFI_AS_STA)
        .WifiCfg = Master::WifiConfig {
            .Mode = Master::WifiOpMode::STA,
            .Ssid = CONFIG_WIFI_SSID,
            .Password = CONFIG_WIFI_PASSWORD,
        },
    #endif
    };

    static Master::App app(cfg);
#elif defined(CONFIG_SCANNER)
    #define MAIN_TAG "Scanner"
    #include "scanner/scanner.h"

    Scanner::AppConfig cfg {
    #if defined(CONFIG_SCANNER_SCAN_CLASSIC_ONLY)
        .Mode = Scanner::ScanMode::ClassicOnly,
    #elif defined(CONFIG_SCANNER_SCAN_BLE_ONLY)
        .Mode = Scanner::ScanMode::BleOnly,
    #else
        .Mode = Scanner::ScanMode::Both,
        .ScanModePeriodClassic = CONFIG_SCANNER_SCAN_BOTH_PERIOD_CLASSIC,
        .ScanModePeriodBle = CONFIG_SCANNER_SCAN_BOTH_PERIOD_BLE,
    #endif
    };

    static Scanner::App app(cfg);
#else
    #error "'MASTER' or 'SCANNER' isn't defined"
#endif

/*
#include <cstdint>
#include <chrono>
#include <numeric>
#include <vector>
#include <variant>

#include "core/bt_common.h"
#include "core/wrapper/gap_ble_wrapper.h"

using namespace Gap::Ble::Type;

struct Receiver : public Gap::Ble::IGapCallback
{
	Receiver()
	    : _wrapper(this)
	{
	}

	void Init()
	{
		Bt::EnableBtController();
		Bt::EnableBluedroid();
		_wrapper.Init();
		_wrapper.StartScanning();
	}

	void GapBleScanResult(const ScanResult & p)
	{
		static auto before = std::chrono::system_clock::now();
		auto now = std::chrono::system_clock::now();
		if ((now - before) > std::chrono::milliseconds(2000)) {
			ESP_LOGI("Rx", "Reseting RSSIs");
			_rssis.clear();
		}
		before = now;

		if (p.adv_data_len == 1 && p.ble_adv[0] == 42) {
			_rssis.push_back(p.rssi);
			const float avg = std::accumulate(_rssis.begin(), _rssis.end(), 0ll)
			                  / static_cast<float>(_rssis.size());
			ESP_LOGI("Rx", "RSSI: %d, Total: %d, Average: %.2f", p.rssi, _rssis.size(), avg);
		}
	}

	void GapBleScanStartCmpl(const ScanStartCmpl &) { ESP_LOGI("Rx", "Scan Start"); }

	void GapBleScanStopCmpl(const ScanStopCmpl &) { ESP_LOGI("Rx", "Scan Stop (Reset required)"); }

	Gap::Ble::Wrapper _wrapper;
	std::vector<std::int8_t> _rssis;
};

struct Transmitter : public Gap::Ble::IGapCallback
{
	Transmitter()
	    : _wrapper(this)
	{
	}

	void Init()
	{
		Bt::EnableBtController();
		Bt::EnableBluedroid();
		_wrapper.Init();
		std::vector<std::uint8_t> data{42};
		_wrapper.SetRawAdvertisingData(data);

		StartAdvOnNext();
	}

	void GapBleScanResult(const Gap::Ble::Type::ScanResult &) {}

	void GapBleAdvStartCmpl(const AdvStartCmpl &) { ESP_LOGI("Tx", "Adv Start"); }

	void GapBleAdvStopCmpl(const AdvStopCmpl &)
	{
		StartAdvOnNext();
		ESP_LOGI("Tx", "Adv Stop (Reset required)");
	}

	void StartAdvOnNext()
	{
		static auto ch = ADV_CHNL_37;
		if (ch == ADV_CHNL_37) {
			ch = ADV_CHNL_38;
		}
		else if (ch == ADV_CHNL_38) {
			ch = ADV_CHNL_39;
		}
		else if (ch == ADV_CHNL_39) {
			ch = ADV_CHNL_37;
		}
		vTaskDelay(pdMS_TO_TICKS(2'000));

		esp_ble_adv_params_t params{};
		params.adv_int_min = 1'000;
		params.adv_int_max = 1'000;
		params.channel_map = ch;
		_wrapper.StartAdvertising(&params, 22.0);
		ESP_LOGI("Tx", "Adv on %d", ch);
	}

	Gap::Ble::Wrapper _wrapper;
};

#ifdef CONFIG_MASTER
static Transmitter app;
#else
static Receiver app;
#endif
*/
extern "C" void app_main(void)
{
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	app.Init();
}
// clang-format on
