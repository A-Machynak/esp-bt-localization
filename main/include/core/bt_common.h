#pragma once

#include <string>
#include <esp_bt_defs.h>

namespace Bt
{

/// @brief Make sure the bt controller is initialized and enabled
void EnableBtController();

/// @brief Make sure the bt controller is uninitialized and disabled
void DisableBtController();

/// @brief Make sure the bluedroid is initialized and enabled
void EnableBluedroid();

/// @brief Make sure the bluedroid is uninitialized and disabled
void DisableBluedroid();

}  // namespace Bt

const char * ToString(const esp_bt_dev_type_t & dType);

const char * ToString(const esp_ble_addr_type_t & aType);
