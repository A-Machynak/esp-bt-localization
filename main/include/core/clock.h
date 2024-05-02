#pragma once

#include <chrono>
#include <cstdint>

namespace Core
{

/// @brief Common clock used across the project
using Clock = std::chrono::system_clock;
using TimePoint = std::chrono::time_point<Clock>;

/// @brief Calculates (after - before)
/// @param before first time point (ex. past)
/// @param after second time point (ex. now)
/// @return difference in milliseconds
std::int64_t DeltaMs(const TimePoint & before, const TimePoint & after = Clock::now());

/// @brief Cast timepoint to unix timestamp (seconds)
/// @param timepoint timepoint
/// @return unix timestamp
std::uint32_t ToUnix(const TimePoint & timepoint);

}  // namespace Core
