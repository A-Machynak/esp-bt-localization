#include "core/bt_common.h"

#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_log.h>

#include <array>
#include <iomanip>
#include <sstream>

namespace
{
/// @brief Logger tag
static const char * TAG = "Bt";

}  // namespace

namespace Bt
{

void EnableBtController()
{
	// Controller
	// Idle -> Init. -> Enable
	#ifdef CONFIG_BTDM_CTRL_MODE_BLE_ONLY
		ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT)); // Saves like 50k
	#endif

	if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
		ESP_LOGI(TAG, "Initializing BT Controller");
		esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
		ESP_ERROR_CHECK(esp_bt_controller_init(&cfg));
	}
	if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) {
		ESP_LOGI(TAG, "Enabling BT Controller");

#ifdef CONFIG_BTDM_CTRL_MODE_BTDM
		ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BTDM));
#else
		ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
#endif
	}
}

void DisableBtController()
{
	// Enabled -> Init. -> Idle
	if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
		ESP_LOGI(TAG, "Disabling BT Controller");
		ESP_ERROR_CHECK(esp_bt_controller_disable());
	}
	if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) {
		ESP_LOGI(TAG, "Deinitializing BT Controller");
		ESP_ERROR_CHECK(esp_bt_controller_deinit());
	}
}

void EnableBluedroid()
{
	// Bluedroid
	// Uninit. -> Init. -> Enable
	if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
		ESP_LOGI(TAG, "Initializing Bluedroid");
		ESP_ERROR_CHECK(esp_bluedroid_init());
	}
	if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_INITIALIZED) {
		ESP_LOGI(TAG, "Enabling Bluedroid");
		ESP_ERROR_CHECK(esp_bluedroid_enable());
	}
}

void DisableBluedroid()
{
	// Enabled -> Init. -> Uninit.
	if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED) {
		ESP_LOGI(TAG, "Disabling Bluedroid");
		ESP_ERROR_CHECK(esp_bluedroid_disable());
	}
	if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_INITIALIZED) {
		ESP_LOGI(TAG, "Deinitializing Bluedroid");
		ESP_ERROR_CHECK(esp_bluedroid_deinit());
	}
}

}  // namespace Bt

const char * ToString(const esp_bt_dev_type_t & dType)
{
	switch (dType) {
	case ESP_BT_DEVICE_TYPE_BREDR:
		return "BR/EDR";
	case ESP_BT_DEVICE_TYPE_BLE:
		return "BLE";
	case ESP_BT_DEVICE_TYPE_DUMO:
		return "DUAL_MODE";
	default:
		return "Unknown";
	}
}

const char * ToString(const esp_ble_addr_type_t & aType)
{
	switch (aType) {
	case BLE_ADDR_TYPE_PUBLIC:
		return "PUBLIC";
	case BLE_ADDR_TYPE_RANDOM:
		return "RANDOM";
	case BLE_ADDR_TYPE_RPA_PUBLIC:
		return "RPA_PUBLIC";
	case BLE_ADDR_TYPE_RPA_RANDOM:
		return "RPA_RANDOM";
	default:
		return "Unknown";
	}
}
