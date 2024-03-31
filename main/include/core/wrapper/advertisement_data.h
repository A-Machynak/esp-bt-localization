#pragma once

#include <array>
#include <cctype>
#include <string>
#include <vector>

namespace Ble
{

/// @brief Builds a BLE advertisement packet.
/// @note BLE allows only 31 octets (bytes) for the advertisement packet
/// ( @see ESP_BLE_ADV_DATA_LEN_MAX in esp_gap_ble_api.h )
class AdvertisementDataBuilder
{
public:
	/// @brief Create a new builder
	/// @return builder
	static AdvertisementDataBuilder Builder();

	/// @brief Complete 128b UUID
	/// @param uuid uuid as a string (format: 01234567-0123-4567-89ab-0123456789ab)
	/// @return *this
	AdvertisementDataBuilder & SetCompleteUuid128(std::string_view uuid);

	/// @brief Complete 128b UUID
	/// @param uuid uuid as an array
	/// @return *this
	AdvertisementDataBuilder & SetCompleteUuid128(const std::array<std::uint8_t, 16> & uuid);

	/// @brief Complete name
	/// @param name device name
	/// @return *this
	AdvertisementDataBuilder & SetCompleteName(std::string_view name);

	/// @brief Finish building and return data
	/// @return raw BLE advertisement
	std::vector<uint8_t> Finish();

private:
	std::vector<uint8_t> _data;

	/// @brief Builder - prevent construction
	AdvertisementDataBuilder();

	/// @brief Builder - prevent copy
	/// @param -
	AdvertisementDataBuilder(const AdvertisementDataBuilder &){};

	/// @brief Builder - prevent assignment
	/// @param -
	/// @return -
	AdvertisementDataBuilder & operator=(const AdvertisementDataBuilder &) { return *this; };
};

}  // namespace Ble
