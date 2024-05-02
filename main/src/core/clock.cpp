#include "core/clock.h"

namespace Core
{

std::int64_t DeltaMs(const TimePoint & before, const TimePoint & after)
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(after - before).count();
}

std::uint32_t ToUnix(const TimePoint & timepoint)
{
	return std::chrono::duration_cast<std::chrono::seconds>(timepoint.time_since_epoch()).count();
}

}  // namespace Core
