#pragma once

#include "core/device_data.h"
#include "core/utility/mac.h"
#include "core/utility/uuid.h"
#include "core/wrapper/device.h"
#include "master/master_cfg.h"
#include "master/memory/device_memory_data.h"
#include "master/memory/idevice_memory.h"

#include "math/matrix.h"

#include <limits>
#include <span>

namespace Master
{

/// @brief Memory for storing and manipulating with devices and scanners.
///
/// "RSSI" is reffered to as "distance", since that's what it represents
/// and is expected to be converted to in the end. Floats aren't used before this point yet,
/// since (according to ESP docs) floating point math is quite a bit slower on ESP.
/// Converting it to distance this early on would also be very limiting.
class DeviceMemory : public IDeviceMemory
{
public:
	/// @brief Constructor
	/// @param cfg configuration
	DeviceMemory(const AppConfig::DeviceMemoryConfig & cfg);

	/// @brief Add new scanner
	/// @param scanner scanner information
	void AddScanner(const ScannerInfo & scanner) override;

	/// @brief Update distance information between a scanner and a device/scanner.
	/// Also used to add new (non-scanner) devices.
	/// @param scanner scanner which received this information
	/// @param device device/scanner data received from Scanner
	/// @{
	void UpdateDistance(const std::uint16_t scannerConnId,
	                    const Core::DeviceDataView::Array & device) override;
	void UpdateDistance(const Mac & scanner, const Core::DeviceDataView::Array & device) override;
	/// @}

	/// @brief Tries to find a scanner, which is missing a distance information in another
	/// scanner and therefore should start advertising. Returns only a single scanner
	/// (since we can't update more at the same time).
	/// @return Scanner information or InvalidScannerIdx, if no scanners are required to advertise
	const ScannerInfo * GetScannerToAdvertise() const override;

	/// @brief Remove scanner
	/// @param connId connection id which this scanner uses
	void RemoveScanner(std::uint16_t connId) override;

	/// @brief Checks whether this device is an already connected scanner
	/// @param bda address
	/// @return true if device is a currently connected scanner
	bool IsConnectedScanner(const Bt::Device & dev) const;

	/// @brief Serializes the output
	/// @return span; valid until the next call of this method
	std::span<std::uint8_t> SerializeOutput();

	/// @brief Reset Scanner positions
	void ResetScannerPositions();

	/// @brief Scanner from connection ID
	/// @param connId connection ID
	/// @return scanner or nullptr if not found
	const ScannerInfo * GetScanner(std::uint16_t connId) const override;

	/// @brief Scanners visitor
	/// @param fn function to call on each scanner
	void VisitScanners(const std::function<void(const ScannerInfo &)> & fn) override;

	/// @brief Device limit.
	static constexpr std::size_t MaximumDevices = 80;

private:
	/// @brief Configuration
	AppConfig::DeviceMemoryConfig _cfg;

	/// @brief RSSIs before being converted to distances - NxN symmetric matrix.
	Math::Matrix<std::int8_t> _scannerRssis;
	/// @brief Scanner distances - NxN symmetric matrix.
	Math::Matrix<float> _scannerDistances;
	/// @brief Resolved scanner positions
	Math::Matrix<float> _scannerPositions;
	bool _scannerPositionsSet = false;

	/// @brief Used as an initial guess for devices
	std::array<float, 3> _scannerCenter{0.0};

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

	/// @brief Update and get the scanner positions.
	/// @return scanner positions or nullptr, if positions cannot be calculated
	/// (not enough data)
	void _UpdateScannerPositions();
	void _UpdateDevicePositions();

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
