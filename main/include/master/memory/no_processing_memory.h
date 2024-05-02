#pragma once

#include "core/clock.h"
#include "master/master_cfg.h"
#include "master/memory/device_memory_data.h"
#include "master/memory/idevice_memory.h"

#include <cstdint>
#include <span>
#include <vector>

namespace
{
constexpr std::size_t GetSerializedDataSize(std::size_t measurements, std::size_t scanners)
{
	return (measurements * (71 + 5 * scanners)) + (scanners * 6) + 6;
}
}  // namespace

namespace Master
{

/// @brief Memory for Scanners and Devices.
/// Only saves measurements and timepoints when they were made.
class NoProcessingMemory : public IDeviceMemory
{
public:
	NoProcessingMemory(const AppConfig::DeviceMemoryConfig & cfg);

	/// @ref IDeviceMemory
	/// @{
	void AddScanner(const ScannerInfo & scanner) override;
	void RemoveScanner(std::uint16_t connId) override;
	const ScannerInfo * GetScanner(std::uint16_t connId) const override;
	void VisitScanners(const std::function<void(const ScannerInfo &)> & fn) override;
	bool IsConnectedScanner(const Bt::Device & dev) const override;
	void UpdateDistance(const std::uint16_t scannerConnId,
	                    const Core::DeviceDataView::Array & devices) override;
	void UpdateDistance(const Mac & scanner, const Core::DeviceDataView::Array & devices) override;
	/// @}

	/// @brief Serialize output:
	///     1B               1B
	/// [Scanner count(N)][Measurement count(M)][Scanners][Measurements]
	///
	/// Scanners(N):
	///  6B
	/// [MAC]
	///
	/// Measurements(M):
	///  6B       N*5B            1B   1B     1B       62B =  71+5*N
	/// [MAC][(Timepoint,RSSI)][Flag][Len][EvtType][AdvData]
	/// @return
	std::span<std::uint8_t> SerializeOutput() override;

	/// @brief Last N measurements saved.
	static constexpr std::size_t MaximumMeasurements = 64;

	/// @brief Maximum scanners
	constexpr static std::size_t MaxScanners = 10;

	/// @brief Maximum size of the output data.
	constexpr static std::size_t MaxSerializedDataSize =
	    GetSerializedDataSize(MaximumMeasurements, 10);

private:
	std::vector<std::uint8_t> _serializedData;
	std::vector<ScannerInfo> _scanners;

	/// @brief Device measurements
	struct NoProcDeviceMeasurements
	{
		struct Measurement
		{
			std::int8_t Rssi{0};           ///< RSSI
			Core::TimePoint LastUpdate{};  ///< Last measurement update

			/// @brief Validity check
			/// @return true - measurement is valid
			bool IsValid() const { return Rssi != 0; };

			/// @brief Invalidate the measurement
			void Invalidate() { Rssi = 0; };
		};

		DeviceInfo Info;                   ///< Device info
		std::array<Measurement, 10> Data;  ///< Measurements from scanners
		std::size_t ValidMeasurements{0};  ///< How many measurements are valid
		Core::TimePoint LastUpdate{};      ///< Last update of last measurement
	};

	std::array<NoProcDeviceMeasurements, MaximumMeasurements> _measurements;

	using ScannerIt = decltype(_scanners)::iterator;
	using MeasurementIt = decltype(_measurements)::iterator;

	void _UpdateDistance(ScannerIt sIt, const Core::DeviceDataView::Array & devices);

	ScannerIt _FindScanner(const Mac & mac);
	MeasurementIt _FindMeasurement(const Mac & mac);
};

}  // namespace Master
