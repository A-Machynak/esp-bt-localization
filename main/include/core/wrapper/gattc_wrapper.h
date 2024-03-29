#pragma once

#include "core/utility/mac.h"
#include "core/wrapper/device.h"
#include "core/wrapper/interface/gattc_if.h"


#include <esp_gatt_common_api.h>
#include <esp_gattc_api.h>


#include <cstdint>
#include <vector>

namespace Gattc
{

/// @brief Application specific info
struct AppInfo
{
	std::uint16_t AppId{};
	std::uint8_t GattIf = ESP_GATT_IF_NONE;
	Gattc::IGattcCallback * Callback{};
};

/// @brief Connection specific info
struct ConnectionInfo
{
	std::uint16_t AppId{};
	std::uint16_t ConnId{};
	std::uint16_t Mtu{};
	Mac Bda;
};

/// @brief Wrapper for BLE GATTs functions to provide more 'C++-like' interface.
/// Application IDs (RegisterApp) and connection IDs (Connect) are required to be managed by
/// the user of this class.
class Wrapper
{
public:
	Wrapper();
	~Wrapper();

	/// @brief Register an application.
	/// @param id application ID
	/// @param callback GATTs callback
	void RegisterApp(std::uint16_t appId, Gattc::IGattcCallback * callback);

	/// @brief Unregister an application. Only an application registered by
	/// `RegisterApp` method can be unregistered.
	/// @param id application ID
	void UnregisterApp(std::uint16_t appId);

	void SetLocalMtu(std::uint16_t mtu);

	/// @brief Write characteristic value.
	/// If the length of the value is greater than the application MTU-3,
	/// the write is split into "Prepare Write"s and "Execute Write" calls
	/// as specified by 'Bt Spec 5.0; ATT 4.9.4 - Write Long Characteristic Values'.
	/// Silently fails if appId or connId is invalid.
	/// @param appId application ID
	/// @param connId connection ID
	/// @param charHandle characteristic handle
	/// @param value value
	void WriteCharVal(std::uint16_t appId,
	                  std::uint16_t connId,
	                  std::uint16_t charHandle,
	                  std::span<std::uint8_t> value);

	/// @brief Write characteristic descriptor value.
	/// If the length of the value is greater than the application MTU-3,
	/// the write is split into "Prepare Write"s and "Execute Write" calls
	/// as specified by 'Bt Spec 5.0; ATT 4.9.4 - Write Long Characteristic Values'.
	/// Silently fails if appId or connId is invalid.
	/// @param appId application ID
	/// @param connId connection ID
	/// @param charHandle characteristic descriptor handle
	/// @param value value
	void WriteCharDescr(std::uint16_t appId,
	                    std::uint16_t connId,
	                    std::uint16_t charHandle,
	                    std::span<std::uint8_t> value);

	/// @brief Open a direct connection to a device.
	/// Silently fails if appId is invalid.
	/// @param appId application ID
	/// @param device device
	void Connect(std::uint16_t appId, const Bt::Device & device);

	/// @brief Open a direct connection to a device.
	/// Silently fails if appId is invalid.
	/// @param appId application ID
	/// @param address device address
	/// @param addrType device address type
	void Connect(std::uint16_t appId, const Mac & address, esp_ble_addr_type_t addrType);

	/// @brief Disconnect from a device
	/// @param appId application ID
	void Disconnect(std::uint16_t appId, std::uint16_t connId);

	/// @brief Search for services
	/// @param appId application ID
	/// @param connId connection ID
	void SearchServices(std::uint16_t appId, std::uint16_t connId);

	/// @brief Search for services
	/// @param appId application ID
	/// @param connId connection ID
	/// @param filter UUID filter (only 2, 4 or 16B long)
	void SearchServices(std::uint16_t appId, std::uint16_t connId, std::span<std::uint8_t> filter);

	void GattcCallback(esp_gattc_cb_event_t event,
	                   esp_gatt_if_t gattIf,
	                   esp_ble_gattc_cb_param_t * param);

	const AppInfo * GetAppInfo(std::uint16_t appId) const;

	const ConnectionInfo * GetConnInfo(std::uint16_t appId, std::uint16_t connId) const;

private:
	std::vector<AppInfo> _apps;
	std::vector<ConnectionInfo> _conns;

	/// @brief Vector for data storage (write requests).
	std::vector<std::uint8_t> _tmpData;

	bool _initialized = false;

	/// @brief Getter
	/// @param appId app id
	/// @return AppInfo
	AppInfo * _GetAppInfo(std::uint16_t appId);

	ConnectionInfo * _GetConnInfo(std::uint16_t appId, std::uint16_t connId);

	/// @brief Common implementation for Write Characteristic Value/Descriptor
	/// @param appId application ID
	/// @param connId connection ID
	/// @param charHandle characteristic value/characteristic descriptor handle
	/// @param value value
	/// @param writeValue true - write characteristic value; false - write characteristic descriptor
	void _WriteChar(std::uint16_t appId,
	                std::uint16_t connId,
	                std::uint16_t charHandle,
	                std::span<std::uint8_t> value,
	                bool writeValue);
};

}  // namespace Gattc

const char * ToString(const esp_gattc_cb_event_t event);
