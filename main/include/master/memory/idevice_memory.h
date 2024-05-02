#pragma once

#include "master/master_cfg.h"
#include "master/memory/device_memory_data.h"
#include "math/matrix.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace Master
{

class IDeviceMemory
{
public:
	/// @brief Constructor
	/// @param cfg configuration
	IDeviceMemory(const AppConfig::DeviceMemoryConfig & cfg)
	    : _cfg(cfg){};

	virtual ~IDeviceMemory(){};

	/// @brief Add/remove/get scanner
	/// @param scanner scanner information
	/// @{
	virtual void AddScanner(const ScannerInfo & scanner) = 0;
	virtual void RemoveScanner(std::uint16_t connId) = 0;
	virtual const ScannerInfo * GetScanner(std::uint16_t connId) const = 0;
	virtual void VisitScanners(const std::function<void(const ScannerInfo &)> & fn) = 0;
	virtual bool IsConnectedScanner(const Bt::Device & dev) const = 0;
	virtual void ResetScannerPositions(){};
	/// @}

	/// @brief Tries to find a scanner, which is missing a distance information in another
	/// scanner and therefore should start advertising. Returns only a single scanner
	/// (since we can't update more at the same time).
	/// @return Scanner information or InvalidScannerIdx, if no scanners are required to advertise
	virtual const ScannerInfo * GetScannerToAdvertise() const { return nullptr; };

	/// @brief Update distance information between a scanner and a device/scanner.
	/// Also used to add new (non-scanner) devices.
	/// @param scanner scanner which received this information
	/// @param device device/scanner data received from Scanner
	/// @{
	virtual void UpdateDistance(const std::uint16_t scannerConnId,
	                            const Core::DeviceDataView::Array & device) = 0;
	virtual void UpdateDistance(const Mac & scanner,
	                            const Core::DeviceDataView::Array & device) = 0;
	/// @}

	/// @brief Serializes the output
	/// @return span; valid until the next call of this method
	virtual std::span<std::uint8_t> SerializeOutput() = 0;

protected:
	AppConfig::DeviceMemoryConfig _cfg;
};
}  // namespace Master
