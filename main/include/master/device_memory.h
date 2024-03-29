#pragma once

#include "core/utility/mac.h"
#include "core/utility/uuid.h"
#include "core/wrapper/device.h"
#include "master/device_memory_data.h"

#include "math/matrix.h"
#include "math/minimizer/functions/anchor_distance.h"
#include "math/minimizer/functions/point_to_anchors.h"
#include "math/minimizer/gradient_minimizer.h"

#include <esp_gatt_defs.h>

#include <limits>
#include <map>
#include <span>
#include <variant>

namespace
{

/// Dimension count
constexpr std::size_t Dimensions = 3;

/// Minimum amount of device measurements to try calculating the device position
constexpr std::size_t MinimumMeasurements = 2;

}  // namespace

namespace Master
{

struct MeasurementData
{
	MeasurementData(std::size_t scannerIdx, std::int8_t rssi);

	std::size_t ScannerIdx;
	std::int8_t Rssi;
};

struct DeviceMeasurements
{
	DeviceMeasurements(const Mac & bda, const MeasurementData & firstMeasurement);

	Mac Bda;
	std::vector<MeasurementData> Data;
	std::array<float, 3> Position;

	static constexpr float InvalidPos = std::numeric_limits<float>::max();
	inline bool IsInvalidPos() const { return Position[0] == InvalidPos; }
};

using ScannerIdx = std::size_t;
constexpr ScannerIdx InvalidScannerIdx = std::numeric_limits<std::size_t>::max();

/// @brief Memory for storing and manipulating with devices and scanners.
///
/// "RSSI" is reffered to as "distance", since that's what it represents
/// and is expected to be converted to in the end. Floats aren't used here yet,
/// since (according to ESP docs) floating point math is quite a bit slower on ESP.
/// Converting it to distance this early on would also be very limiting.
class DeviceMemory
{
public:
	DeviceMemory();

	/// @brief Add new scanner
	/// @param scanner scanner information
	void AddScanner(const ScannerInfo & scanner);

	/// @brief Update distance information between a scanner and a device/scanner.
	/// Also used to add new distance information.
	/// @param scanner scanner which received this information
	/// @param device device or scanner
	/// @param rssi RSSI, aka "distance"
	/// @{
	void UpdateDistance(const Mac & scanner,
	                    std::span<const Mac> device,
	                    std::span<const std::int8_t> rssi);
	void UpdateDistance(std::uint16_t scannerConnId,
	                    std::span<const Mac> device,
	                    std::span<const std::int8_t> rssi);
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

	const std::vector<ScannerInfo> & GetScanners() const;

	std::span<std::uint8_t> SerializeOutput();

private:
	/// Positions and distance matrices
	/// @{
	Math::Matrix<float> _scannerDistances;
	Math::Matrix<float> _scannerPositions;
	/// @}
	bool _scannerDistancesSet = false;

	/// Connected scanners, devices and maps for BDA lookup.
	/// @{
	std::vector<ScannerInfo> _scanners;
	std::vector<DeviceMeasurements> _devices;
	std::map<Mac, std::size_t, Mac::Cmp> _scannerMap;
	std::map<Mac, std::size_t, Mac::Cmp> _deviceMap;
	/// @}
	std::vector<std::uint8_t> _serializedData;

	/// @brief Update distance information between a scanner and devices/scanners
	/// @param scanner scanner
	/// @param devices devices and/or scanners
	/// @param rssis distances
	void _UpdateDistance(std::size_t scannerIdx,
	                     std::span<const Mac> devices,
	                     std::span<const std::int8_t> rssis);

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

	// [ 000 | 0.0 | 0.0 | 0.0 ]
	// [ XXX | 000 | 0.0 | 0.0 ]
	// [ XXX | XXX | 000 | 0.0 ]
	// [ XXX | XXX | XXX | 000 ]

	// [ 000 | 0.0 | 0.0 ]
	// [ XXX | 000 | 0.0 ]
	// [ XXX | XXX | 000 ]
};

}  // namespace Master
