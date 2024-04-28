#pragma once

#include "core/utility/mac.h"
#include "core/wrapper/device.h"
#include "core/wrapper/gap_ble_wrapper.h"
#include "core/wrapper/gattc_wrapper.h"
#include "core/wrapper/interface/gap_ble_if.h"
#include "core/wrapper/interface/gattc_if.h"

#include "master/http/api/post_data.h"
#include "master/http/server.h"
#include "master/master_cfg.h"
#include "master/memory/idevice_memory.h"

#include <optional>
#include <span>
#include <vector>

#include <freertos/semphr.h>

namespace Master::Impl
{

/// @brief Master application implementation
class App final
    : public Gattc::IGattcCallback
    , public Gap::Ble::IGapCallback
{
public:
	/// @brief Constructor
	App(const AppConfig & cfg);
	~App();

	/// @brief Initialize with configuration
	/// @param config config
	void Init();

	/// @brief HTTP server API POST callback
	/// @param data data sent in POST request
	void OnHttpServerUpdate(std::span<const char> data);

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
	/// @brief Configuration
	AppConfig _cfg;

	/// @brief GAP/GATTc/HTTP
	/// @{
	Gap::Ble::Wrapper _bleGap;         ///< BLE GAP
	Gattc::Wrapper _gattc;             ///< GATTc
	const Gattc::AppInfo * _gattcApp;  ///< GATTc Application info
	::Master::HttpServer _httpServer;
	/// @}

	/// @brief Temporary storage
	/// @{
	std::vector<ScannerInfo> _tmpScanners;
	std::vector<std::uint8_t> _tmpSerializedData;
	/// @}

	/// @brief Stored scanners and devices' data
	IDeviceMemory * _memory;
	SemaphoreHandle_t _memMutex;

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

	/// @brief Process system message
	/// @param op operation
	void _ProcessSystemMessage(HttpApi::Type::SystemMsg::Operation op);
};

}  // namespace Master::Impl
