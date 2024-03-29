#include "core/gatt_common.h"

const char * ToString(const esp_gatt_conn_reason_t & reason)
{
	switch (reason) {
	case ESP_GATT_CONN_UNKNOWN:
		return "UNKNOWN";
	case ESP_GATT_CONN_L2C_FAILURE:
		return "L2C_FAILURE";
	case ESP_GATT_CONN_TIMEOUT:
		return "TIMEOUT";
	case ESP_GATT_CONN_TERMINATE_PEER_USER:
		return "TERMINATE_PEER_USER";
	case ESP_GATT_CONN_TERMINATE_LOCAL_HOST:
		return "TERMINATE_LOCAL_HOST";
	case ESP_GATT_CONN_FAIL_ESTABLISH:
		return "FAIL_ESTABLISH";
	case ESP_GATT_CONN_LMP_TIMEOUT:
		return "LMP_TIMEOUT";
	case ESP_GATT_CONN_CONN_CANCEL:
		return "CONN_CANCEL";
	case ESP_GATT_CONN_NONE:
		return "NONE";
	default:
		return "";
	}
}