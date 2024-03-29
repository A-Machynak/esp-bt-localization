#pragma once

#include <cstdint>

#include <esp_gatts_api.h>

#include "core/utility/mac.h"
#include "core/wrapper/device.h"
#include "core/wrapper/gatt_attribute_table.h"
#include "core/wrapper/interface/gatts_if.h"


namespace Gatts
{

/// @brief Application specific info
struct AppInfo
{
	std::uint16_t AppId{};
	esp_gatt_if_t GattIf = ESP_GATT_IF_NONE;
	Gatts::IGattsCallback * Callback{};
	std::vector<std::size_t> ServiceIndices;
	std::vector<std::uint16_t> GattHandles;
};

/// @brief Connection specific info
struct ConnectionInfo
{
	std::uint16_t AppId{};
	std::uint16_t ConnId{};
	Mac Bda;
};

/// @brief Write 'transaction' (sequence of prepare writes ended with an execute).
struct Transaction
{
	std::uint16_t AppId{};                ///< Application ID
	std::uint16_t ConnId{};               ///< Connection ID
	std::uint16_t Handle{};               ///< Attribute handle
	std::vector<std::uint8_t> WriteData;  ///< Data
};

/// @brief Wrapper for BLE GATTs functions to provide more 'C++-like' interface
/// A lot of methods are missing, since they aren't used in this project.
class Wrapper
{
public:
	/// @brief Constructor.
	/// @param legacyWrite if false, `Prepare Write` and `Execute Write` events,
	/// aka long writes, will be processed and merged into a single `Execute Write` event.
	/// Responses will be processed automatically.
	Wrapper(bool legacyWrite = false);
	~Wrapper();

	/// @brief Register an application.
	/// @param appId application id
	/// @param callback GATTs callback
	void RegisterApp(std::uint16_t appId, Gatts::IGattsCallback * callback);

	/// @brief Unregister an application.
	/// @param appId application id
	void UnregisterApp(std::uint16_t appId);

	/// @brief Close connection
	/// @param connId connection ID
	void Close(std::uint16_t appId, std::uint16_t connId);

	/// @brief Create attribute table for an application.
	/// Can only be called after GATTs register event; otherwise it'll fail.
	/// @param appIdx app index (depends on the order in which it was registered)
	/// @param attributeTable attribute table
	/// @param serviceIndices services in the attribute table to start
	void CreateAttributeTable(std::uint16_t appId,
	                          const Gatt::AttributeTable & attributeTable,
	                          const std::vector<std::size_t> & serviceIndices);

	/// @brief Sets the MTU.
	/// @param mtu MTU; clamps to valid values
	void SetLocalMtu(std::uint16_t mtu);

	/// @brief GATTs callback. Not meant to be called directly.
	/// @param event event
	/// @param gattIf interface
	/// @param param params
	void GattsCallback(esp_gatts_cb_event_t event,
	                   esp_gatt_if_t gattIf,
	                   esp_ble_gatts_cb_param_t * param);

	/// @brief Getter
	/// @param appId application id
	/// @return AppInfo
	const AppInfo * GetAppInfo(std::uint16_t appId) const;

private:
	/// @brief Applications
	std::vector<AppInfo> _apps;

	/// @brief Connections
	std::vector<ConnectionInfo> _conns;

	/// @brief Initialization
	bool _initialized = false;

	/// @brief Merging prepare/execute writes
	bool _legacyWrite = false;

	/// @brief Temporary data for prepare/execute writes.
	/// These are not really transactions in the BT sense,
	/// since each prepare write is considered a new transaction.
	std::vector<Transaction> _transactions;

	/// @brief Getter
	/// @param appId application id
	/// @return AppInfo
	AppInfo * _GetAppInfo(std::uint16_t appId);

	/// @brief Getter
	/// @param appId application id
	/// @param connId connection id
	/// @param transId transaction id
	/// @return Transaction
	Transaction * _GetTransaction(std::uint16_t appId, std::uint16_t connId, std::uint32_t transId);

	void _GattsWrite(AppInfo & app, const Gatts::Type::Write & p);
	void _GattsExecWrite(AppInfo & app, const Gatts::Type::ExecWrite & p);
};

}  // namespace Gatts

const char * ToString(const esp_gatts_cb_event_t event);
