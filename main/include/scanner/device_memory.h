#pragma once

#include <chrono>
#include <cstdint>
#include <span>
#include <vector>

#include "core/device_data.h"
#include "core/wrapper/device.h"
#include "scanner/device_memory_data.h"
#include "scanner/scanner_cfg.h"

namespace Scanner
{

/// @brief Device memory for scanner
class DeviceMemory
{
public:
	/// @brief Constructor
	/// @param sizeLimit maximum amount of devices to hold
	DeviceMemory(const AppConfig::DeviceMemoryConfig & cfg);

	/// @brief Add new device
	/// @param device device info
	void AddDevice(const Bt::Device & device);

	/// @brief Serialize data - only up to 512B. This is destructive - serialized data will be
	/// removed.
	/// @param[out] output output destination. Will be cleared and resized
	void SerializeData(std::vector<std::uint8_t> & out);

	/// @brief Manually remove stale devices.
	/// This method might be called during in other public methods when required,
	/// so it isn't necessary, to be called manually.
	void RemoveStaleDevices();

private:
	/// @brief Configuration
	const AppConfig::DeviceMemoryConfig & _cfg;

	/// @brief Device data
	std::vector<DeviceInfo> _devData;

	/// @brief Attempt to associate a device to another device
	/// @param device device info
	/// @return true if device was associated and added to the data vector
	bool _AssociateDevice(const Bt::Device & device);
};

}  // namespace Scanner
