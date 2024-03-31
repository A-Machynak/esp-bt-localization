#pragma once

#include <cstdint>

namespace Scanner
{

/// @brief Scanning/Discovery mode
enum class ScanMode
{
	ClassicOnly,
	BleOnly,
	Both
};

/// @brief Scanner application configuration
struct AppConfig
{
	ScanMode Mode;
	std::uint16_t ScanModePeriod;  ///< Scanning period for ScanMode::Both
};
}  // namespace Scanner