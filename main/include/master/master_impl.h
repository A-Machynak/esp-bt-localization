#pragma once

#include "core/utility/mac.h"
#include "core/wrapper/device.h"
#include "core/wrapper/gap_ble_wrapper.h"
#include "core/wrapper/gattc_wrapper.h"
#include "core/wrapper/interface/gap_ble_if.h"
#include "core/wrapper/interface/gattc_if.h"
#include "master/device_memory.h"
#include "master/http/server.h"
#include "master/master_cfg.h"

#include <optional>
#include <vector>

#include <freertos/semphr.h>

namespace
{

/// @brief Maximum number of scanners.
constexpr std::size_t ScannerConnectionLimit = 32 /*MASTER_MAX_SCANNER_CONNECTIONS*/;

}  // namespace

namespace Master::Impl
{

/// @brief Master application implementation
class App final
    : public Gattc::IGattcCallback
    , public Gap::Ble::IGapCallback
{
public:
	/// @brief Constructor
	App();

	/// @brief Initialize with configuration
	/// @param config config
	void Init(const AppConfig & config);

	/// @brief BLE GAP related callbacks
	/// @{
	void GapBleScanResult(const Gap::Ble::Type::ScanResult & p) override;
	void GapBleScanStopCmpl(const Gap::Ble::Type::ScanStopCmpl & p) override;
	void GapBleUpdateConn(const Gap::Ble::Type::UpdateConn & p) override;
	/// @}

	/// @brief GATTc related callbacks
	/// @{
	void GattcReg(const Gattc::Type::Reg & p) override;
	void GattcOpen(const Gattc::Type::Open & p) override;
	void GattcClose(const Gattc::Type::Close & p) override;
	void GattcDisconnect(const Gattc::Type::Disconnect & p) override;
	void GattcCancelOpen() override;
	void GattcReadChar(const Gattc::Type::ReadChar & p) override;
	void GattcSearchCmpl(const Gattc::Type::SearchCmpl & p) override;
	void GattcSearchRes(const Gattc::Type::SearchRes & p) override;
	/// @}

	/// @brief Update loop. Uses a dedicated task.
	void UpdateScannersLoop();

	/// @brief Update loop. Uses a dedicated task.
	void UpdateDeviceDataLoop();

private:
	Gap::Ble::Wrapper _bleGap;         ///< BLE GAP
	Gattc::Wrapper _gattc;             ///< GATTc
	const Gattc::AppInfo * _gattcApp;  ///< GATTc Application info
	::Master::HttpServer _httpServer;

	/// @brief Temporary storage
	/// @{
	std::vector<ScannerInfo> _tmpScanners;
	std::vector<std::uint8_t> _tmpSerializedData;
	/// @}

	SemaphoreHandle_t _memMutex;

	/// @brief Stored scanners and devices' data
	DeviceMemory _memory;

	/// @brief BDA of a scanner that we want to connect to.
	/// Since we have to wait for BLE scan to stop before connecting,
	/// we have to save it and connect later.
	std::optional<Mac> _scannerToConnect = std::nullopt;

	/// @brief Task handles
	/// @{
	TaskHandle_t _updateScannersTask;
	TaskHandle_t _readDevTask;
	/// @}

	bool _IsScanner(const Bt::Device & p);
	void _ScanForScanners();
};

}  // namespace Master::Impl
