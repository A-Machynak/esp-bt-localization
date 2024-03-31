#pragma once

#include <chrono>
#include <cstdint>
#include <span>
#include <vector>

#include "core/device_data.h"
#include "core/wrapper/device.h"
#include "scanner/device_memory_data.h"

namespace Scanner
{

/// @brief Maximum interval in milliseconds between updates, after which the device is removed
constexpr std::int64_t StaleLimit = 30'000;

/// @brief Whether or not to enable BLE device association.
/// - BLE devices with random BDA and same advertising data will be resolved as one device.
constexpr bool DefaultEnableAssociation = true;

/// @brief Device memory for scanner
class DeviceMemory
{
public:
	/// @brief Constructor
	/// @param sizeLimit maximum amount of devices to hold
	DeviceMemory(std::size_t sizeLimit = Core::DefaultMaxDevices);

	/// @brief Add new device
	/// @param device device info
	void AddDevice(const Bt::Device & device);

	/// @brief Serialize data
	/// @param[out] output output destination. Will be cleared an resized
	void SerializeData(std::vector<std::uint8_t> & out);

	/// @brief Manually remove stale devices.
	/// This method might be called during in other public methods when required,
	/// so it isn't necessary, to be called manually.
	void RemoveStaleDevices();

	/// @brief Enable or disable device association.
	/// - BLE devices with random BDA and same advertising data will be
	/// resolved as one device.
	/// @param enabled true - enable; false - disable; default: false
	void SetAssociation(bool enabled);

private:
	/// @brief Maximum device limit
	const std::size_t _sizeLimit;

	/// @brief Enable/disable association
	bool _association{DefaultEnableAssociation};

	/// @brief Device data
	std::vector<DeviceInfo> _devData;

	/// @brief Attempt to associate a device to another device
	/// @param device device info
	/// @return true if device was associated and added to the data vector
	bool _AssociateDevice(const Bt::Device & device);
};

}  // namespace Scanner
