#pragma once

#include "scanner/scanner_cfg.h"
#include <sdkconfig.h>

// clang-format off
constexpr Scanner::AppConfig Cfg
{
#if defined(CONFIG_SCANNER_SCAN_CLASSIC_ONLY)
	.Mode = Scanner::ScanMode::ClassicOnly,
#elif defined(CONFIG_SCANNER_SCAN_BLE_ONLY)
	.Mode = Scanner::ScanMode::BleOnly,
#else
	.Mode = Scanner::ScanMode::Both,
	.ScanModePeriodClassic = CONFIG_SCANNER_SCAN_BOTH_PERIOD_CLASSIC,
	.ScanModePeriodBle = CONFIG_SCANNER_SCAN_BOTH_PERIOD_BLE,
#endif
	.DeviceMemoryCfg = {
		.MemorySizeLimit = CONFIG_SCANNER_DEVICE_COUNT_LIMIT,
#if defined(CONFIG_SCANNER_ENABLE_ASSOCIATION)
		.EnableAssociation = true,
#else
		.EnableAssociation = false,
#endif
		.StaleLimit = CONFIG_SCANNER_STALE_LIMIT,
		.MinRssi = CONFIG_SCANNER_MIN_RSSI
	}
};
// clang-format on
