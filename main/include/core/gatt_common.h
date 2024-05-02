#pragma once

#include "core/utility/uuid.h"

#include <esp_gatt_defs.h>
#include <esp_gatts_api.h>

#include <string>

namespace Gatt
{
/// @brief Primary service UUID
static constexpr std::uint16_t PrimaryService = ESP_GATT_UUID_PRI_SERVICE;

/// @brief Characteristic declaration UUID
static constexpr std::uint16_t CharacteristicDeclaration = ESP_GATT_UUID_CHAR_DECLARE;

/// @brief Scanner service characteristic count
constexpr std::uint16_t ScannerServiceCharCount = 3;

/// @brief UUID of the scanner service
constexpr std::string_view ScannerService = "7e6bf038-0f00-47ab-a215-cf841f4289f0";
constexpr std::array ScannerServiceArray = Util::UuidToArray(ScannerService);
constexpr esp_bt_uuid_t ScannerServiceStruct = Util::UuidToStruct(ScannerService);

/// @brief UUID of the 'state' characteristic (scanner service)
constexpr std::string_view StateCharacteristic = "7e6bf038-0f00-47ab-a215-cf841f4289f1";
constexpr std::array StateCharacteristicArray = Util::UuidToArray(StateCharacteristic);
constexpr esp_bt_uuid_t StateCharacteristicStruct = Util::UuidToStruct(StateCharacteristic);

/// @brief UUID of the 'devices' characteristic (scanner service)
constexpr std::string_view DevicesCharacteristic = "7e6bf038-0f00-47ab-a215-cf841f4289f2";
constexpr std::array DevicesCharacteristicArray = Util::UuidToArray(DevicesCharacteristic);
constexpr esp_bt_uuid_t DevicesCharacteristicStruct = Util::UuidToStruct(DevicesCharacteristic);

/// @brief UUID of the 'timestamp' characteristic (scanner service)
constexpr std::string_view TimestampCharacteristic = "7e6bf038-0f00-47ab-a215-cf841f4289f3";
constexpr std::array TimestampCharacteristicArray = Util::UuidToArray(TimestampCharacteristic);
constexpr esp_bt_uuid_t TimestampCharacteristicStruct = Util::UuidToStruct(TimestampCharacteristic);

/// @brief State characteristic values
enum StateChar : std::uint8_t
{
	Idle = 0,
	Advertise = 1,
	Scan = 2
};

}  // namespace Gatt

const char * ToString(const esp_gatt_conn_reason_t & reason);
