#pragma once

#include <cstdint>

#include "core/wrapper/gap_ble_wrapper.h"
#include "core/wrapper/gap_bt_wrapper.h"
#include "core/wrapper/gatt_attribute_table.h"
#include "core/wrapper/gatts_wrapper.h"
#include "core/wrapper/interface/gap_ble_if.h"
#include "core/wrapper/interface/gap_bt_if.h"
#include "core/wrapper/interface/gatts_if.h"

#include "scanner/device_memory.h"
#include "scanner/scanner_cfg.h"

#include <freertos/semphr.h>

namespace Scanner::Impl
{

/// @brief Handle indices
enum Handle : std::size_t
{
	Service = 0,
	StateDecl = 1,
	State = 2,
	DevicesDecl = 3,
	Devices = 4
};

/// @brief Connection status
enum class ConnectionStatus
{
	Connected,
	Disconnected
};

/// @brief Scanner application implementation
class App final
    : public Gatts::IGattsCallback
    , public Gap::Ble::IGapCallback
    , public Gap::Bt::IGapCallback
{
public:
	/// @brief Constructor.
	App(const AppConfig & cfg);

	/// @brief Initialize with configuration
	/// @param cfg config
	void Init();

	/// @brief BLE GAP related callbacks
	/// @{
	void GapBleScanResult(const Gap::Ble::Type::ScanResult & p) override;
	void GapBleAdvStopCmpl(const Gap::Ble::Type::AdvStopCmpl & p) override;
	/// @}

	/// @brief BT GAP related callbacks
	/// @{
	void GapBtDiscRes(const Gap::Bt::DiscRes & p) override;
	void GapBtDiscStateChanged(const Gap::Bt::DiscStateChanged & p) override;
	/// @}

	/// @brief GATTs related callbacks
	/// @{
	void GattsRegister(const Gatts::Type::Register &) override;
	void GattsConnect(const Gatts::Type::Connect & p) override;
	void GattsDisconnect(const Gatts::Type::Disconnect & p) override;
	void GattsRead(const Gatts::Type::Read & p) override;
	void GattsWrite(const Gatts::Type::Write & p) override;
	/// @}

private:
	AppConfig _cfg;

	/// @brief Chrono typedefs
	/// @{
	using Clock = std::chrono::system_clock;
	using TimePoint = std::chrono::time_point<Clock>;
	/// @}

	/// @brief GAP/GATTS
	/// @{
	Gap::Ble::Wrapper _bleGap;
	Gap::Bt::Wrapper _btGap;
	Gatts::Wrapper _gatts;
	const Gatts::AppInfo * _appInfo;
	/// @}

	/// @brief State
	Gatt::StateChar _state;

	/// @brief Memory for storing devices
	DeviceMemory _memory;
	SemaphoreHandle_t _memMutex;

	/// @brief Vector for serializing data
	std::vector<std::uint8_t> _serializeVec;

	/// @brief GATT attribute table
	/// Scanner Service
	///  - State characteristic declaration
	///   - State characteristic value
	///  - Devices characteristic declaration
	///   - Devices characteristic value
	Gatt::AttributeTable _attributeTable = std::move(
	    Gatt::AttributeTableBuilder::Build()
	        .Service(Gatt::ScannerService)
	        .Declaration(ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ)
	        .Value(Gatt::StateCharacteristic, 1, 1, ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE)
	        .Declaration(ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_READ)
	        .Value(Gatt::DevicesCharacteristic,
	               0,
	               Core::DeviceMemoryByteSize(),
	               ESP_GATT_PERM_READ,
	               ESP_GATT_RSP_BY_APP)  // Using custom logic
	        .Finish());

	/// @brief Connection state
	ConnectionStatus _connStatus = ConnectionStatus::Disconnected;

	/// @brief Last time the `Devices` GATT attribute was updated
	TimePoint _lastDevicesUpdate = Clock::now();

	/// @brief Changes state - start/stop advertising/scanning
	/// @param state new state
	void _ChangeState(const Gatt::StateChar state);

	/// @brief Advertise/Scan
	/// @{
	void _AdvertiseDefault();
	void _AdvertiseToBeacons();
	void _ScanForDevices();
	/// @}

	/// Checks if `Devices` data (GATTS attribute) needs to be updated and updates it if so
	void _CheckAndUpdateDevicesData();
	/// Forces `Devices` data (GATTS attribute) update
	void _UpdateDevicesData();
};

}  // namespace Scanner::Impl
