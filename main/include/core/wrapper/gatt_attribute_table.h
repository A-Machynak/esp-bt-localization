#pragma once

#include <esp_gatt_defs.h>
#include <esp_log.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/gatt_common.h"
#include "core/utility/uuid.h"

namespace Gatt
{

/// @brief Wrapper over GATT attribute table.
/// Create new services, declarations and values using Service(), Declaration() and Value()
/// methods respectively. The order of the methods matters, since their indices will
/// be the same as the handles returned by `esp_ble_gatts_create_attr_tab`.
class AttributeTable
{
public:
	using Item = std::unique_ptr<std::vector<std::uint8_t>>;

	/// @brief Add a new service
	/// @param uuid128 128b uuid
	/// @param attControl automatic (from BT stack) or manual response (from application)
	/// to read/write requests
	void Service(std::string_view uuid128, std::uint8_t attControl = ESP_GATT_AUTO_RSP);

	/// @brief Add a new characteristic declaration
	/// @param properties properties
	/// @param attControl automatic (from BT stack) or manual response (from application)
	/// to read/write requests
	void Declaration(esp_gatt_char_prop_t properties, std::uint8_t attControl = ESP_GATT_AUTO_RSP);

	/// @brief Add new characteristic value
	/// @param uuid128 128b uuid
	/// @param length current length
	/// @param maxLength maximum length
	/// @param permissions permissions
	/// @param attControl automatic (from BT stack) or manual response (from application)
	/// to read/write requests
	void Value(std::string_view uuid128,
	           std::uint16_t length,
	           std::uint16_t maxLength,
	           esp_gatt_perm_t permissions,
	           std::uint8_t attControl = ESP_GATT_AUTO_RSP);

	/// @brief Stored attributes
	std::vector<Item> Attributes;

	/// @brief Stored UUIDs
	std::vector<Item> Uuids;

	/// @brief GATT attribute database
	std::vector<esp_gatts_attr_db_t> Db;
};

/// @brief Attribute table builder.
class AttributeTableBuilder
{
public:
	/// @brief Create builder
	/// @return builder
	static AttributeTableBuilder Build();

	/// @brief Add new Service
	/// @param uuid128 128b custom UUID
	/// @param attControl automatic (from BT stack) or manual response (from application)
	/// to read/write requests
	/// @return this
	AttributeTableBuilder & Service(std::string_view uuid128,
	                                std::uint8_t attControl = ESP_GATT_AUTO_RSP);

	/// @brief Add new Characteristic Declaration
	/// @param properties characteristic properties
	/// @param attControl automatic (from BT stack) or manual response (from application)
	/// to read/write requests
	/// @return this
	AttributeTableBuilder & Declaration(esp_gatt_char_prop_t properties,
	                                    std::uint8_t attControl = ESP_GATT_AUTO_RSP);

	/// @brief Add new Characteristic Value
	/// @param uuid128 128b custom UUID
	/// @param length characteristic length
	/// @param maxLength maximum characteristic length
	/// @param permissions perms
	/// @param attControl automatic (from BT stack) or manual response (from application)
	/// to read/write requests
	/// @return this
	AttributeTableBuilder & Value(std::string_view uuid128,
	                              std::uint16_t length,
	                              std::uint16_t maxLength,
	                              esp_gatt_perm_t permissions,
	                              std::uint8_t attControl = ESP_GATT_AUTO_RSP);

	/// @brief Finish building and return table.
	/// @return table
	AttributeTable && Finish();

private:
	AttributeTable _table;
};

}  // namespace Gatt
