#pragma once

#include "master/master_cfg.h"
#include "master/memory/device_memory_data.h"
#include "master/memory/idevice_memory.h"

#include <cstdint>
#include <span>
#include <vector>

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

	/// @brief Measurement limit.
	static constexpr std::size_t MaximumMeasurements = 128;

private:
	std::vector<std::uint8_t> _serializedData;
	std::vector<ScannerInfo> _scanners;

	/// @brief
	struct NoProcDeviceMeasurements
	{
		DeviceInfo Info;                    ///< Device info
		std::vector<MeasurementData> Data;  ///< Measurements from scanners
	};

	std::vector<NoProcDeviceMeasurements> _measurements;

	using ScannerIt = std::vector<ScannerInfo>::iterator;
	using MeasurementIt = std::vector<NoProcDeviceMeasurements>::iterator;

	void _UpdateDistance(ScannerIt sIt, const Core::DeviceDataView::Array & devices);

	ScannerIt _FindScanner(const Mac & mac);
	MeasurementIt _FindMeasurement(const Mac & mac);
};

}  // namespace Master
