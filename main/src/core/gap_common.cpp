#include "core/gap_common.h"


const char * ToString(const esp_gap_search_evt_t & p)
{
	switch (p) {
	case ESP_GAP_SEARCH_INQ_RES_EVT:
		return "INQ_RES_EVT";
	case ESP_GAP_SEARCH_INQ_CMPL_EVT:
		return "INQ_CMPL_EVT";
	case ESP_GAP_SEARCH_DISC_RES_EVT:
		return "DISC_RES_EVT";
	case ESP_GAP_SEARCH_DISC_BLE_RES_EVT:
		return "DISC_BLE_RES_EVT";
	case ESP_GAP_SEARCH_DISC_CMPL_EVT:
		return "DISC_CMPL_EVT";
	case ESP_GAP_SEARCH_DI_DISC_CMPL_EVT:
		return "DI_DISC_CMPL_EVT";
	case ESP_GAP_SEARCH_SEARCH_CANCEL_CMPL_EVT:
		return "SEARCH_CANCEL_CMPL_EVT";
	case ESP_GAP_SEARCH_INQ_DISCARD_NUM_EVT:
		return "INQ_DISCARD_NUM_EVT";
	default:
		return "";
	}
}

