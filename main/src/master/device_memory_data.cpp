#include "master/device_memory_data.h"

#include <algorithm>

namespace Master
{

void DeviceOut::Serialize(std::span<std::uint8_t> output) const
{
	Serialize(output, Bda, Position, Flags);
}

void DeviceOut::Serialize(std::span<std::uint8_t> output,
                          std::span<const std::uint8_t, 6> bda,
                          std::span<const float, 3> position,
                          const bool isScanner,
                          const bool isBle,
                          const bool isPublic)
{
	const std::uint8_t flags =
	    (isScanner ? 0b0000'0001 : 0) | (isBle ? 0b0000'0010 : 0) | (isPublic ? 0b0000'0100 : 0);
	Serialize(output, bda, position, static_cast<FlagMask>(flags));
}

void DeviceOut::Serialize(std::span<std::uint8_t> output,
                          std::span<const std::uint8_t, 6> bda,
                          std::span<const float, 3> position,
                          const FlagMask flags)
{
	assert(output.size() >= Size);

	std::copy_n(bda.data(), bda.size(), output.data());

	// Probably the only way to serialize float without UB?
	union FloatU {
		float F;
		std::uint8_t V[sizeof(float)];
	};

	constexpr std::size_t PosOffset = 6;
	for (std::size_t i = 0; i < position.size(); i++) {
		auto * p = output.data() + PosOffset + i * sizeof(float);
		FloatU f{position[i]};
		for (std::size_t j = 0; j < sizeof(float); j++) {
			p[j] = f.V[j];
		}
	}

	constexpr std::size_t FlagsOffset = PosOffset + 3 * sizeof(float);
	output[FlagsOffset] = static_cast<std::uint8_t>(flags);

	static_assert(Size == 19);  // Make sure we change this method, if the structure changes
}
}  // namespace Master
