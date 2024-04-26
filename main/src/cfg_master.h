#pragma once

#include <esp_eap_client.h>

#include "master/master.h"
#include <cstdint>
#include <sdkconfig.h>

#if defined(CONFIG_WIFI_VALIDATE_WPA2_SERVER)
extern uint8_t CaPemStart[] asm("_binary_ca_pem_start");
extern uint8_t CaPemEnd[] asm("_binary_ca_pem_end");
#endif

#if defined(CONFIG_WIFI_WPA2_EAP_METHOD_TLS)
extern uint8_t ClientCrtStart[] asm("_binary_client_crt_start");
extern uint8_t ClientCrtEnd[] asm("_binary_client_crt_end");

extern uint8_t ClientKeyStart[] asm("_binary_client_key_start");
extern uint8_t ClientKeyEnd[] asm("_binary_client_key_end");
#endif

// clang-format off
const Master::AppConfig Cfg
{
	.GattReadInterval = CONFIG_MASTER_GATT_READ_INTERVAL,
	.DelayBetweenGattReads = CONFIG_MASTER_DELAY_BETWEEN_GATT_READS,
	.DeviceMemoryCfg = Master::AppConfig::DeviceMemoryConfig {
		.MinMeasurements = CONFIG_MASTER_MIN_MEASUREMENTS,
		.MinScanners = CONFIG_MASTER_MIN_SCANNERS,
		.MaxScanners = CONFIG_MASTER_MAX_SCANNERS,
		.DeviceStoreTime = CONFIG_MASTER_DEVICE_STORE_TIME,
		.DefaultPathLoss = CONFIG_MASTER_DEFAULT_PATH_LOSS,
		.DefaultEnvFactor = CONFIG_MASTER_DEFAULT_ENV_FACTOR,
#if defined(CONFIG_MASTER_NO_POSITION_CALCULATION)
		.NoPositionCalculation = true,
#else
		.NoPositionCalculation = false,
#endif
	},
	.WifiCfg = Master::WifiConfig{
#if defined(CONFIG_WIFI_AS_AP)
		.Mode = Master::WifiOpMode::AP,
#else
		.Mode = Master::WifiOpMode::STA,
#endif // CONFIG_WIFI_AS_AP
		.Ap = Master::ApConfig {
			.Ssid = CONFIG_WIFI_AP_SSID,
			.Password = CONFIG_WIFI_AP_PASSWORD,
			.Channel = CONFIG_WIFI_AP_CHANNEL,
			.MaxConnections = CONFIG_WIFI_AP_MAX_CONNECTIONS,
		},
		.Sta = Master::StaConfig {
			.Ssid = CONFIG_WIFI_STA_SSID,
			.Password = CONFIG_WIFI_STA_PASSWORD,
			.MaxRetryCount = 0,
#if defined(CONFIG_WIFI_USE_WPA2)
			.UseWpa2Enterprise = true,
#if defined(CONFIG_WIFI_VALIDATE_WPA2_SERVER)
			.ValidateWpa2Server = true,
			.CaPem = std::span(CaPemStart, CaPemEnd-CaPemStart),
#else
			.ValidateWpa2Server = false,
			.CaPem = {},
#endif // CONFIG_WIFI_VALIDATE_WPA2_SERVER
#if defined(CONFIG_WIFI_WPA2_EAP_METHOD_TLS)
			.ClientCrt = std::span(ClientCrtStart, ClientCrtEnd-ClientCrtStart),
			.ClientKey = std::span(ClientKeyStart, ClientKeyEnd-ClientKeyStart),
#else
			.ClientCrt = {},
			.ClientKey = {},
#endif // CONFIG_WIFI_WPA2_EAP_METHOD_TLS
#if defined(CONFIG_WIFI_WPA2_EAP_METHOD_PEAP)
			.EapMethod_ = Master::EapMethod::Peap,
#elif defined(CONFIG_WIFI_WPA2_EAP_METHOD_TTLS)
			.EapMethod_ = Master::EapMethod::Ttls,
#else
			.EapMethod_ = Master::EapMethod::Tls,
#endif // CONFIG_WIFI_WPA2_EAP_METHOD_PEAP
#if defined(CONFIG_WIFI_WPA2_EAP_METHOD_TTLS_PHASE2_MSCHAPV2)
			.Phase2Eap = esp_eap_ttls_phase2_types::ESP_EAP_TTLS_PHASE2_MSCHAPV2,
#elif defined(CONFIG_WIFI_WPA2_EAP_METHOD_TTLS_PHASE2_MSCHAP)
			.Phase2Eap = esp_eap_ttls_phase2_types::ESP_EAP_TTLS_PHASE2_MSCHAP,
#elif defined(CONFIG_WIFI_WPA2_EAP_METHOD_TTLS_PHASE2_PAP)
			.Phase2Eap = esp_eap_ttls_phase2_types::ESP_EAP_TTLS_PHASE2_PAP,
#elif defined(CONFIG_WIFI_WPA2_EAP_METHOD_TTLS_PHASE2_CHAP)
			.Phase2Eap = esp_eap_ttls_phase2_types::ESP_EAP_TTLS_PHASE2_CHAP,
#else
			.Phase2Eap = esp_eap_ttls_phase2_types::ESP_EAP_TTLS_PHASE2_EAP,
#endif
			.EapId = CONFIG_WIFI_WPA2_EAP_ID,
			.EapUsername = CONFIG_WIFI_WPA2_EAP_USERNAME,
			.EapPassword = CONFIG_WIFI_WPA2_EAP_PASSWORD,
#else
			.UseWpa2Enterprise = false,
			.ValidateWpa2Server = {},
			.CaPem = {},
			.ClientCrt = {},
			.ClientKey = {},
			.EapMethod_ = {},
			.Phase2Eap = {},
			.EapId = {},
			.EapUsername = {},
			.EapPassword = {},
#endif // CONFIG_WIFI_USE_WPA2
		},
	},
};
// clang-format on