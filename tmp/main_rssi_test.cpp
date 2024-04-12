#pragma once

#include <cstdint>
#include <numeric>
#include <vector>

#include <esp_log.h>

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
		_wrapper.Init();
		_wrapper.StartScanning();
	}

	void GapBleScanResult(const ScanResult & p)
	{
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
		_wrapper.Init();
		std::vector<std::uint8_t> data{42};
		_wrapper.SetRawAdvertisingData(data);

		esp_ble_adv_params_t params{
		    .adv_int_min = 480,
		    .adv_int_max = 500,
		};
		_wrapper.StartAdvertising(&params, 10.0);
	}

	void GapBleScanResult(const Gap::Ble::Type::ScanResult &) {}

	void GapBleAdvStartCmpl(const AdvStartCmpl &) { ESP_LOGI("Tx", "Adv Start"); }

	void GapBleAdvStopCmpl(const AdvStopCmpl &) { ESP_LOGI("Tx", "Adv Stop (Reset required)"); }

	Gap::Ble::Wrapper _wrapper;
};

#ifdef MASTER
static Transmitter app;
#else
static Receiver app;
#endif
extern "C" void app_main(void)
{
	app.Init();
}