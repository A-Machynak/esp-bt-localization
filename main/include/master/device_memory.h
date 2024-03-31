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

/// Dimension count
constexpr std::size_t Dimensions = 3;

/// Minimum amount of device measurements to try calculating the device position
constexpr std::size_t MinimumMeasurements = 2;

/// Minimum amount of scanners to calculate their relative positions
constexpr std::size_t MinimumScanners = 2;

}  // namespace

namespace Master
{
/// @brief Index type
using ScannerIdx = std::size_t;

/// @brief Invalid index
constexpr ScannerIdx InvalidScannerIdx = std::numeric_limits<ScannerIdx>::max();

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
	/// @return Scanner information or nullptr, if no scanners are required to advertise
	ScannerIdx GetScannerIdxToAdvertise() const;

	/// @brief Remove scanner
	/// @param connId connection id which this scanner uses
	void RemoveScanner(std::uint16_t connId);

	/// @brief Checks whether this device is an already connected scanner and returns it
	/// @param bda address
	/// @return scanner or nullptr, if it isn't a connected scanner
	ScannerIdx GetConnectedScannerIdx(const Bt::Device & dev) const;

	/// @brief Whether or not a scanner should start advertising - that is, another scanner
	/// is missing distance information to this scanner
	/// @param scanner scanner to check
	/// @return true - should start advertising, because another scanner is missing distance
	/// information
	bool ShouldAdvertise(ScannerIdx scannerId);

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
	/// @brief Scanner distances - NxN symmetric matrix.
	Math::Matrix<float> _scannerDistances;
	/// @brief Resolved scanner positions
	Math::Matrix<float> _scannerPositions;
	bool _scannerPositionsSet = false;

	/// @brief Used as an initial guess for devices
	std::array<float, 3> _scannerCenter{0.0, 0.0, 0.0};

	/// Connected scanners, devices and maps for BDA lookup.
	/// @{
	std::vector<ScannerDetail> _scanners;
	std::vector<DeviceMeasurements> _devices;
	std::map<Mac, std::size_t, Mac::Cmp> _scannerMap;
	std::map<Mac, std::size_t, Mac::Cmp> _deviceMap;
	/// @}

	/// @brief For raw data serialization
	std::vector<std::uint8_t> _serializedData;

	/// @brief Update distance information between a scanner and devices/scanners
	/// @param scanner scanner
	/// @param devices devices and/or scanners
	void _UpdateDistance(ScannerIdx scannerIdx, const Core::DeviceDataView::Array & devices);

	/// @brief Update distance information to a device
	/// @param scannerIdx scanner
	/// @param devIdx device
	/// @param rssi distance
	void _UpdateDevice(std::size_t scannerIdx, std::size_t devIdx, std::int8_t rssi);

	/// @brief Update distance information between 2 scanners
	/// @param sIdx1 first scanner
	/// @param sIdx2 second scanner
	/// @param rssi distance
	void _UpdateScanner(std::size_t sIdx1, std::size_t sIdx2, std::int8_t rssi);

	/// @brief Remove scanner and measurements related to it
	/// @param scannerIdx index to _scanners
	void _RemoveScanner(std::size_t scannerIdx);

	/// @brief Remove measurements related to scanner
	/// @param scannerIdx index to _scanners
	void _RemoveDeviceMeasurements(std::size_t scannerIdx);

	void _UpdateScannerCenter();
};

}  // namespace Master
