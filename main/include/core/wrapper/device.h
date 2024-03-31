#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include <esp_bt_defs.h>
#include <esp_gap_ble_api.h>
#include <esp_gap_bt_api.h>

#include "core/utility/mac.h"
#include "core/wrapper/interface/gap_ble_if.h"
#include "core/wrapper/interface/gap_bt_if.h"


namespace Bt
{
/// @brief Invalid RSSI in case that BR/EDR doesn't contain it (how/why?)
constexpr std::int8_t InvalidRssi = std::numeric_limits<std::int8_t>::max();

/// @brief Classic (BR/EDR) or BLE device type
enum class DeviceType
{
	BrEdr,
	Ble
};

/// @brief Bt Classic specific data from discovery result
struct BrEdrSpecific
{
	/// @brief Constructor
	/// @param dr BT Discovery result
	BrEdrSpecific(const Gap::Bt::DiscRes & dr);

	/// @brief Bluetooth device name
	std::string DeviceName;

	/// @brief COD bits.
	/// @see esp_bt_gap_get_cod_srvc, esp_bt_gap_get_cod_minor_dev, etc...
	std::uint32_t ClassOfDevice;

	/// @brief Device RSSI
	std::int8_t Rssi;

	/// @brief Raw EIR data
	std::vector<std::uint8_t> Eir;
};

/// @brief BLE specific data from scan result
struct BleSpecific
{
	/// @brief Constructor
	/// @param sr BLE Scan result
	BleSpecific(const Gap::Ble::ScanResult & sr);

	/// @brief Inquiry/Discovery ...
	esp_gap_search_evt_t SearchEvt;

	/// @brief Address type (Public/Random)
	esp_ble_addr_type_t AddrType;

	/// @brief Event type.
	esp_ble_evt_type_t EvtType;

	/// @brief RSSI
	std::int8_t Rssi;

	/// @brief Advertising data length
	std::uint8_t AdvDataLen = 0;

	/// @brief Scan response data length
	std::uint8_t ScanRspLen = 0;

	/// @brief EIR data wrapper
	struct Eir
	{
		/// @brief EIR max size
		static constexpr std::size_t Size =
		    ESP_BLE_ADV_DATA_LEN_MAX + ESP_BLE_SCAN_RSP_DATA_LEN_MAX;

		/// @brief Single EIR record
		struct Record
		{
			esp_ble_adv_data_type Type;
			std::span<const std::uint8_t> Data;
		};

		/// @brief Constructor
		/// @param data data
		Eir(const std::array<std::uint8_t, Eir::Size> & data);

		/// @brief Underlying EIR data
		std::array<std::uint8_t, Eir::Size> Data;

		/// @brief Pointers records in EIR data. See @ref esp_ble_adv_data_type to parse this data.
		std::vector<Record> Records;

		/// @brief Parse EIR records. All this does is save all the types and pointers to data into
		/// a vector of EirRecords.
		/// @param eir data; it needs to outlive the returned vector
		/// @return vector of records
		static std::vector<Record> ParseEirRecords(const std::array<std::uint8_t, Eir::Size> & eir);
	};

	/// @brief EIR data
	Eir EirData;
};

/// @brief Common device structure for a BR/EDR (Classic) or a BLE device.
struct Device
{
	/// @brief Constructor for a device from a BR/EDR discovery result
	/// @param dr discovery result
	Device(const Gap::Bt::DiscRes & dr);

	/// @brief Constructor for a device from a BLE scan result
	/// @param sr scan result
	Device(const Gap::Ble::ScanResult & sr);

	/// @brief Bluetooth device address
	Mac Bda;

	/// @brief Classic/BLE device specific data
	std::variant<BrEdrSpecific, BleSpecific> Data;

	/// @brief Convenience getters
	/// @{
	const BrEdrSpecific & GetBrEdr() const;
	const BleSpecific & GetBle() const;
	std::int8_t GetRssi() const;
	bool IsBrEdr() const;
	bool IsBle() const;
	/// @}
};

}  // namespace Bt

std::string ToString(const Bt::Device & addr);
