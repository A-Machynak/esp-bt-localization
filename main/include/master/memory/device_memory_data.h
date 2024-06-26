#pragma once

#include "core/clock.h"
#include "core/device_data.h"
#include "core/utility/mac.h"
#include "core/wrapper/device.h"

#include <esp_gatt_defs.h>

#include <array>
#include <cstdint>
#include <span>

namespace Master
{

/// @brief Single scanner
struct ScannerInfo
{
	/// @brief Handles for scanner service
	struct ServiceInfo
	{
		std::uint16_t StartHandle{ESP_GATT_INVALID_HANDLE};    ///< Service start handle
		std::uint16_t EndHandle{ESP_GATT_INVALID_HANDLE};      ///< Service end handle
		std::uint16_t StateChar{ESP_GATT_INVALID_HANDLE};      ///< 'State' char. handle
		std::uint16_t DevicesChar{ESP_GATT_INVALID_HANDLE};    ///< 'Devices' char. handle
		std::uint16_t TimestampChar{ESP_GATT_INVALID_HANDLE};  ///< 'Timestamp' char. handle
	};

	std::uint16_t ConnId;  ///< Connection id
	Mac Bda;               ///< Bluetooth device address
	ServiceInfo Service;   ///< Service info
};

/// @brief Internal scanner info for DeviceMemory
struct ScannerDetail
{
	/// @brief Constructor
	/// @param info scanner info
	ScannerDetail(const ScannerInfo & info);

	ScannerInfo Info;            ///< Info
	Core::TimePoint LastUpdate;  ///< Time of the last update

	/// @brief How many other scanners' measurements were used to approximate this
	/// scanner's position
	std::uint8_t UsedMeasurements{0};
};

/// @brief Measurement data for a device
struct MeasurementData
{
	/// @brief Constructor
	/// @param scannerIdx index of the scanner which created this measurement
	/// @param rssi RSSI
	MeasurementData(std::size_t scannerIdx, std::int8_t rssi);

	std::size_t ScannerIdx;      ///< Scanner index which owns this measurement
	std::int8_t Rssi;            ///< RSSI
	Core::TimePoint LastUpdate;  ///< Last measurement update
};

/// @brief Device info. Similar to `DeviceData`; but without the RSSI
struct DeviceInfo
{
	Mac Bda;                               ///< Bluetooth device address
	std::uint8_t Flags;                    ///< BLE device? Public BDA?
	std::uint8_t AdvDataSize;              ///< Advertising data size
	esp_ble_evt_type_t EventType;          ///< Event type
	std::array<std::uint8_t, 62> AdvData;  ///< Advertising data

	bool IsBle() const { return Flags & 0b1; }
	bool IsAddrTypePublic() const { return Flags & 0b10; }
};

/// @brief Collection of measurements for a device from multiple scanners.
struct DeviceMeasurements
{
	/// @brief Constructor
	/// @param data view
	/// @param firstMeasurement first measurement
	DeviceMeasurements(const Core::DeviceDataView & data, const MeasurementData & firstMeasurement);

	DeviceInfo Info;                    ///< Device info
	std::vector<MeasurementData> Data;  ///< Measurements from scanners
	std::array<float, 3> Position;      ///< Resolved position (possibly invalid)
	Core::TimePoint LastUpdate;         ///< Last time a measurement was received

	static constexpr float InvalidPos = std::numeric_limits<float>::max();
	inline bool IsInvalidPos() const { return Position[0] == InvalidPos; }
};

/// @brief Output device data
struct DeviceOut
{
	enum class FlagMask : std::uint8_t
	{
		IsScanner = 0b0000'0001,
		IsBle = 0b0000'0010,
		IsAddrTypePublic = 0b0000'0100
	};
	std::array<std::uint8_t, 6> Bda;  ///< Bluetooth device address
	std::array<float, 3> Position;    ///< The (X,Y,Z) position
	std::uint8_t ScannerCount;        ///< Scanner measurements used to approximate position
	FlagMask Flags;                   ///< Flags. @ref FlagsMask

	/// @brief Data indices
	/// @{
	constexpr static std::size_t BdaIdx = 0;
	constexpr static std::size_t PositionIdx = 6;
	constexpr static std::size_t ScannerCountIdx = 18;
	constexpr static std::size_t FlagsIdx = 19;
	/// @}
	constexpr static std::size_t Size = 20;

	/// @brief Serialize
	/// @param[out] output output destination
	void Serialize(std::span<std::uint8_t, Size> output) const;

	/// @brief Serialize
	/// @param[out] output output destination
	/// @param[in] bda bluetooth device address
	/// @param[in] position x,y,z position
	/// @param[in] scannerCount number of scanner measurements used to approximate the position
	/// @param[in] flags flags
	static void Serialize(std::span<std::uint8_t, Size> output,
	                      std::span<const std::uint8_t, 6> bda,
	                      std::span<const float, 3> position,
	                      std::uint8_t scannerCount,
	                      const FlagMask flags);

	/// @brief Serialize
	/// @param[out] output output destination
	/// @param[in] bda bluetooth device address
	/// @param[in] position x,y,z position
	/// @param[in] scannerCount number of scanner measurements used to approximate the position
	/// @param[in] isScanner is this device a scanner?
	/// @param[in] isBle is this device a BLE device?
	/// @param[in] isPublic is this device's BDA public?
	static void Serialize(std::span<std::uint8_t, Size> output,
	                      std::span<const std::uint8_t, 6> bda,
	                      std::span<const float, 3> position,
	                      std::uint8_t scannerCount,
	                      const bool isScanner,
	                      const bool isBle,
	                      const bool isPublic);
};

}  // namespace Master
