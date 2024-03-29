
#include "core/wrapper/advertisement_data.h"
#include "core/utility/uuid.h"

#include <esp_gap_ble_api.h>

namespace Ble
{

AdvertisementDataBuilder::AdvertisementDataBuilder()
{
	// we can't really have more than ESP_BLE_ADV_DATA_LEN_MAX
	_data.reserve(ESP_BLE_ADV_DATA_LEN_MAX);
}

AdvertisementDataBuilder AdvertisementDataBuilder::Builder()
{
	return AdvertisementDataBuilder();
}

AdvertisementDataBuilder & AdvertisementDataBuilder::SetCompleteUuid128(std::string_view uuid)
{
	return SetCompleteUuid128(Util::UuidToArray(uuid, false));
}

AdvertisementDataBuilder & AdvertisementDataBuilder::SetCompleteUuid128(const std::array<std::uint8_t, 16> & uuid)
{
	_data.push_back(uuid.size() + 1); // Length
	_data.push_back(esp_ble_adv_data_type::ESP_BLE_AD_TYPE_128SRV_CMPL); // Type

	std::copy(uuid.begin(), uuid.end(), std::back_inserter(_data));
	return *this;
}

AdvertisementDataBuilder & AdvertisementDataBuilder::SetCompleteName(std::string_view name)
{
	assert(name.size() < ESP_BLE_ADV_DATA_LEN_MAX);

	// Complete name
	// [Length, Type, Name]
	const esp_ble_adv_data_type type = esp_ble_adv_data_type::ESP_BLE_AD_TYPE_NAME_CMPL;
	const uint8_t length = name.size() + 1;
	_data.reserve(length + 1);

	_data.push_back(length);
	_data.push_back(static_cast<uint8_t>(type));
	for (size_t i = 0; i < name.size(); i++) {
		_data.push_back(name[i]);
	}
	return *this;
}

std::vector<uint8_t> AdvertisementDataBuilder::Finish()
{
	assert(_data.size() <= ESP_BLE_ADV_DATA_LEN_MAX);

	_data.shrink_to_fit();
	return _data;
}

}  // namespace Ble