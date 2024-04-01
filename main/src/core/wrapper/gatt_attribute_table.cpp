
#include "core/wrapper/gatt_attribute_table.h"

#include <esp_log.h>

namespace Gatt
{

void AttributeTable::Service(std::string_view uuid128, std::uint8_t attControl)
{
	const std::array arr = Util::UuidToArray(uuid128);

	Attributes.emplace_back(std::make_unique<std::vector<std::uint8_t>>(arr.begin(), arr.end()));
	esp_gatts_attr_db_t entry{.attr_control = attControl,
	                          .att_desc = {.uuid_length = ESP_UUID_LEN_16,
	                                       .uuid_p = (uint8_t *)&PrimaryService,
	                                       .perm = ESP_GATT_PERM_READ,
	                                       .max_length = (uint16_t)Attributes.back()->size(),
	                                       .length = (uint16_t)Attributes.back()->size(),
	                                       .value = Attributes.back()->data()}};
	Db.push_back(entry);
}

void AttributeTable::Declaration(esp_gatt_char_prop_t properties, std::uint8_t attControl)
{
	auto vec = std::make_unique<std::vector<std::uint8_t>>();
	vec->push_back(properties);
	Attributes.push_back(std::move(vec));

	esp_gatts_attr_db_t entry{// Characteristic declaration
	                          .attr_control = attControl,
	                          .att_desc = {.uuid_length = ESP_UUID_LEN_16,
	                                       .uuid_p = (std::uint8_t *)&CharacteristicDeclaration,
	                                       .perm = ESP_GATT_PERM_READ,
	                                       .max_length = 1,
	                                       .length = 1,
	                                       .value = Attributes.back()->data()}};
	Db.push_back(entry);
}

void AttributeTable::Value(std::string_view uuid128,
                           std::uint16_t length,
                           std::uint16_t maxLength,
                           esp_gatt_perm_t permissions,
                           std::uint8_t attControl)
{
	const std::array uuidArr = Util::UuidToArray(uuid128);
	auto vecUuid = std::make_unique<std::vector<std::uint8_t>>(uuidArr.begin(), uuidArr.end());
	auto vecData = std::make_unique<std::vector<std::uint8_t>>();
	vecData->resize(maxLength);
	Uuids.push_back(std::move(vecUuid));
	Attributes.push_back(std::move(vecData));

	// Characteristic value
	esp_gatts_attr_db_t db{.attr_control = attControl,
	                       .att_desc = {.uuid_length = ESP_UUID_LEN_128,
	                                    .uuid_p = Uuids.back()->data(),
	                                    .perm = permissions,
	                                    .max_length = std::min(maxLength, (std::uint16_t)512),
	                                    .length = length,
	                                    .value = Attributes.back()->data()}};
	Db.push_back(db);
}

AttributeTableBuilder AttributeTableBuilder::Build()
{
	return AttributeTableBuilder();
}

AttributeTableBuilder & AttributeTableBuilder::Service(std::string_view uuid128,
                                                       std::uint8_t attControl)
{
	_table.Service(uuid128, attControl);
	return *this;
}

AttributeTableBuilder & AttributeTableBuilder::Declaration(esp_gatt_char_prop_t properties,
                                                           std::uint8_t attControl)
{
	_table.Declaration(properties, attControl);
	return *this;
}

AttributeTableBuilder & AttributeTableBuilder::Value(std::string_view uuid128,
                                                     std::uint16_t length,
                                                     std::uint16_t maxLength,
                                                     esp_gatt_perm_t permissions,
                                                     std::uint8_t attControl)
{
	_table.Value(uuid128, length, maxLength, permissions, attControl);
	return *this;
}

AttributeTable && AttributeTableBuilder::Finish()
{
	return std::move(_table);
}
}  // namespace Gatt