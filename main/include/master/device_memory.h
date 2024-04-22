#pragma once

#include "core/device_data.h"
#include "core/utility/mac.h"
#include "core/utility/uuid.h"
#include "core/wrapper/device.h"
#include "master/device_memory_data.h"

#include "math/matrix.h"

#include <chrono>
#include <limits>
#include <map>
#include <span>

namespace
{

/// @brief Dimension count
constexpr std::size_t Dimensions = 3;

/// @brief Minimum amount of device measurements to try calculating the device position
constexpr std::size_t MinimumMeasurements = 2;

/// @brief Minimum amount of scanners to calculate their relative positions
constexpr std::size_t MinimumScanners = 3;

/// @brief How long before a device gets removed if it doens't receive a measurement. [ms]
constexpr std::int64_t DeviceRemoveTimeMs = 60'000;

/// @brief Hard device limit.
constexpr std::size_t MaximumDevices = 128;

/// @brief Soft device limit. This much will be preallocated.
/// Adding more devices might result in reallocation fail and restart.
constexpr std::size_t MaximumDevicesSoft = MaximumDevices;

/// @brief Hard scanner limit.
constexpr std::size_t MaximumScanners = 10;

/// @brief Soft scanner limit. This much will be preallocated.
/// Adding more scanners might result in reallocation fail and restart.
constexpr std::size_t MaximumScannersSoft = 10;

}  // namespace

namespace Master
{

/// @brief Memory for storing and manipulating with devices and scanners.
///
/// "RSSI" is reffered to as "distance", since that's what it represents
/// and is expected to be converted to in the end. Floats aren't used before this point yet,
/// since (according to ESP docs) floating point math is quite a bit slower on ESP.
/// Converting it to distance this early on would also be very limiting.
class DeviceMemory
{
public:
	/// @brief Constructor
	DeviceMemory();

	/// @brief Add new scanner
	/// @param scanner scanner information
	void AddScanner(const ScannerInfo & scanner);

	/// @brief Update distance information between a scanner and a device/scanner.
	/// Also used to add new (non-scanner) devices.
	/// @param scanner scanner which received this information
	/// @param device device/scanner data received from Scanner
	/// @{
	void UpdateDistance(const std::uint16_t scannerConnId,
	                    const Core::DeviceDataView::Array & device);
	void UpdateDistance(const Mac & scanner, const Core::DeviceDataView::Array & device);
	/// @}

	/// @brief Tries to find a scanner, which is missing a distance information in another
	/// scanner and therefore should start advertising. Returns only a single scanner
	/// (since we can't update more at the same time).
	/// @return Scanner information or InvalidScannerIdx, if no scanners are required to advertise
	const ScannerInfo * GetScannerToAdvertise() const;

	/// @brief Remove scanner
	/// @param connId connection id which this scanner uses
	void RemoveScanner(std::uint16_t connId);

	/// @brief Checks whether this device is an already connected scanner
	/// @param bda address
	/// @return true if device is a currently connected scanner
	bool IsConnectedScanner(const Bt::Device & dev) const;

	/// @brief Update and get the scanner positions.
	/// @return scanner positions or nullptr, if positions cannot be calculated
	/// (not enough data)
	const Math::Matrix<float> * UpdateScannerPositions();
	const std::vector<DeviceMeasurements> & UpdateDevicePositions();

	/// @brief Serializes the output
	/// @return span; valid until the next call of this method
	std::span<std::uint8_t> SerializeOutput();

	/// @brief Scanner from connection ID
	/// @param connId connection ID
	/// @return scanner or nullptr if not found
	const ScannerDetail * GetScanner(std::uint16_t connId) const;

	/// @brief Scanners getter
	/// @return all scanners
	const std::vector<ScannerDetail> & GetScanners() const;

private:
	/// @brief RSSIs before being converted to distances - NxN symmetric matrix.
	Math::Matrix<std::int8_t> _scannerRssis;
	/// @brief Scanner distances - NxN symmetric matrix.
	Math::Matrix<float> _scannerDistances;
	/// @brief Resolved scanner positions
	Math::Matrix<float> _scannerPositions;
	bool _scannerPositionsSet = false;

	/// @brief Used as an initial guess for devices
	std::array<float, Dimensions> _scannerCenter{0.0};

	/// @brief Connected scanners and devices.
	/// @{
	std::vector<ScannerDetail> _scanners;
	std::vector<DeviceMeasurements> _devices;
	using ScannerIt = std::vector<ScannerDetail>::iterator;
	using DeviceIt = std::vector<DeviceMeasurements>::iterator;
	/// @}

	/// @brief For raw data serialization
	std::vector<std::uint8_t> _serializedData;

	/// @brief Find a scanner/device
	/// @param mac BDA
	/// @return scanner/device iterator
	/// @{
	ScannerIt _FindScanner(const Mac & mac);
	DeviceIt _FindDevice(const Mac & mac);
	/// @}

	/// @brief Add new device with maximum size checking.
	/// @param device new device data
	void _AddDevice(DeviceMeasurements device);

	/// @brief Update distance information between a scanner and devices/scanners
	/// @param sIt scanner
	/// @param devices devices and/or scanners
	void _UpdateDistance(ScannerIt sIt, const Core::DeviceDataView::Array & devices);

	/// @brief Update distance information to a device
	/// @param sIt scanner
	/// @param devIt device
	/// @param rssi distance
	void _UpdateDevice(ScannerIt sIt, DeviceIt devIt, std::int8_t rssi);

	/// @brief Update distance information between 2 scanners
	/// @param sIt1 first scanner
	/// @param sIt2 second scanner
	/// @param rssi distance
	void _UpdateScanner(ScannerIt sIt1, ScannerIt sIt2, std::int8_t rssi);

	/// @brief Remove scanner and measurements related to it
	/// @param sIt iterator from _scanners
	void _RemoveScanner(ScannerIt sIt);

	/// @brief Remove measurements; called after a scanner disconnects.
	void _ResetDeviceMeasurements();

	/// @brief Remove old devices.
	void _RemoveStaleDevices();

	/// @brief Update center of the scanners.
	void _UpdateScannerCenter();
};

}  // namespace Master
