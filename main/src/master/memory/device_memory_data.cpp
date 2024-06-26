#include "master/memory/device_memory_data.h"

#include <algorithm>

namespace Master
{

MeasurementData::MeasurementData(std::size_t scannerIdx, std::int8_t rssi)
    : ScannerIdx(scannerIdx)
    , Rssi(rssi)
    , LastUpdate(Core::Clock::now())
{
}

DeviceMeasurements::DeviceMeasurements(const Core::DeviceDataView & data,
                                       const MeasurementData & firstMeasurement)
    : Info({data.Mac(), data.Flags(), data.AdvDataSize(), data.EventType(), {}})
    , Data({firstMeasurement})
    , Position{InvalidPos, InvalidPos, InvalidPos}
    , LastUpdate(Core::Clock::now())
{
	std::copy(data.AdvData().begin(), data.AdvData().end(), Info.AdvData.begin());
}

ScannerDetail::ScannerDetail(const ScannerInfo & info)
    : Info(info)
    , LastUpdate(Core::Clock::now())
{
}

void DeviceOut::Serialize(std::span<std::uint8_t, Size> output) const
{
	Serialize(output, Bda, Position, ScannerCount, Flags);
}

void DeviceOut::Serialize(std::span<std::uint8_t, Size> output,
                          std::span<const std::uint8_t, 6> bda,
                          std::span<const float, 3> position,
                          std::uint8_t scannerCount,
                          const bool isScanner,
                          const bool isBle,
                          const bool isPublic)
{
	const std::uint8_t flags =
	    (isScanner ? 0b0000'0001 : 0) | (isBle ? 0b0000'0010 : 0) | (isPublic ? 0b0000'0100 : 0);
	Serialize(output, bda, position, scannerCount, static_cast<FlagMask>(flags));
}

void DeviceOut::Serialize(std::span<std::uint8_t, Size> output,
                          std::span<const std::uint8_t, 6> bda,
                          std::span<const float, 3> position,
                          std::uint8_t scannerCount,
                          const FlagMask flags)
{
	std::copy_n(bda.data(), bda.size(), output.data() + BdaIdx);

	// Probably the only way to serialize float without UB?
	union FloatU {
		float F;
		std::uint8_t V[sizeof(float)];
	};

	for (std::size_t i = 0; i < position.size(); i++) {
		auto * p = output.data() + PositionIdx + i * sizeof(float);
		FloatU f{position[i]};
		for (std::size_t j = 0; j < sizeof(float); j++) {
			p[j] = f.V[j];
		}
	}
	output[ScannerCountIdx] = scannerCount;
	output[FlagsIdx] = static_cast<std::uint8_t>(flags);

	static_assert(Size == 20);  // Make sure we change this method, if the structure changes
}
}  // namespace Master
