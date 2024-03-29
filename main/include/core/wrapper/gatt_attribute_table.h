#pragma once

#include <esp_gatt_defs.h>
#include <esp_log.h>

#include <array>
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
	void Service(std::string_view uuid128);

	/// @brief Add a new characteristic declaration
	/// @param properties properties
	void Declaration(esp_gatt_char_prop_t properties);

	/// @brief Add new characteristic value
	/// @param uuid128 128b uuid
	/// @param length current length
	/// @param maxLength maximum length
	/// @param permissions permissions
	void Value(std::string_view uuid128,
	           std::uint16_t length,
	           std::uint16_t maxLength,
	           esp_gatt_perm_t permissions);

	/// @brief Stored attributes
	std::vector<Item> Attributes;

	/// @brief Stored UUIDs
	std::vector<Item> Uuids;

	/// @brief GATT attribute database
	std::vector<esp_gatts_attr_db_t> Db;
};

class AttributeTableBuilder
{
public:
	static AttributeTableBuilder Build();

	AttributeTableBuilder & Service(std::string_view uuid128);

	AttributeTableBuilder & Declaration(esp_gatt_char_prop_t properties);

	AttributeTableBuilder & Value(std::string_view uuid128,
	                              std::uint16_t length,
	                              std::uint16_t maxLength,
	                              esp_gatt_perm_t permissions);

	AttributeTable && Finish();

private:
	AttributeTable _table;
};

}  // namespace Gatt
