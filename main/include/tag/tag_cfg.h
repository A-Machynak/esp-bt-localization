#pragma once

#include <cstdint>

namespace Tag
{

struct AppConfig
{
	/// @brief Minimum advertising interval (N * 0.625msec)
	std::uint16_t AdvIntMin{400};

	/// @brief Maximum advertising interval (N * 0.625msec)
	std::uint16_t AdvIntMax{800};

	/// @brief Advertise on all channels or specific ones.
	bool AdvertiseOnAllChannels{false};

	/// @brief Advertising channel to advertise on if not on all (37/38/39)
	std::uint8_t ChannelToAdvertiseOn{37};
};

}  // namespace Tag
