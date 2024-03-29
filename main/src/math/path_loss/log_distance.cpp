#include "math/path_loss/log_distance.h"

#include <cmath>

namespace PathLoss
{

float LogDistance(std::int8_t rssi, float envFactor, std::int8_t refPathLoss)
{
	// d = d0 + 10^((PL - PLref) / 10*n)
	return std::pow(10.0, (-rssi - refPathLoss) / (10.0 * envFactor));
}

}  // namespace PathLoss
