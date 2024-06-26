#pragma once

#include <cstdint>

namespace PathLoss
{
/// @brief Default parameters
/// @{
constexpr float DefaultEnvFactor = 4.0;
constexpr std::int8_t DefaultRefPathLoss = 45;
/// @}

/// @brief Log-distance path loss model.
/// @param rssi received signal strength (possibly with subtracted transmit power)
/// @param envFactor environmental factor
/// @param refPathLoss reference path loss (at 1 meter)
/// @return distance
float LogDistance(std::int8_t rssi,
                  float envFactor = DefaultEnvFactor,
                  std::int8_t refPathLoss = DefaultRefPathLoss);

}  // namespace PathLoss
