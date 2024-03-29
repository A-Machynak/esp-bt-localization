#pragma once

#include <cstdint>

namespace PathLoss
{

/// @brief Log-distance path loss model.
/// @param rssi received signal strength (possibly with subtracted transmit power)
/// @param envFactor environmental factor
/// @param refPathLoss reference path loss (at 1 meter)
/// @return distance
float LogDistance(std::int8_t rssi, float envFactor = 4.0, std::int8_t refPathLoss = 50);

}  // namespace PathLoss
