#include "core/wrapper/device.h"

#include "core/bt_common.h"
#include "core/gap_common.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace Bt
{

BrEdrSpecific::BrEdrSpecific(const Gap::Bt::DiscRes & dr)
{
	constexpr int MaximumLength = 1024;

	// Parse properties. Move this somewhere else perhaps
	for (int i = 0; i < dr.num_prop; i++) {
		esp_bt_gap_dev_prop_t & prop = dr.prop[i];
		if (prop.len <= 0 || MaximumLength < prop.len) {  // Sanity check
			continue;
		}

		// Parse property
		switch (prop.type) {
		case ESP_BT_GAP_DEV_PROP_BDNAME: {
			DeviceName = std::string(reinterpret_cast<char *>(prop.val), prop.len);
			break;
		}
		case ESP_BT_GAP_DEV_PROP_COD: {
			std::uint32_t cod = *reinterpret_cast<std::uint32_t *>(prop.val);
			if (esp_bt_gap_is_valid_cod(cod)) {
				ClassOfDevice = cod;
			}
			break;
		}
		case ESP_BT_GAP_DEV_PROP_RSSI: {
			Rssi = *reinterpret_cast<std::int8_t *>(prop.val);
			break;
		}
		case ESP_BT_GAP_DEV_PROP_EIR: {
			auto * p = reinterpret_cast<std::uint8_t *>(prop.val);
			std::copy_n(p, prop.len, std::back_inserter(Eir));
			break;
		}
		}
	}
}

BleSpecific::BleSpecific(const Gap::Ble::ScanResult & sr)
    : SearchEvt(sr.search_evt)
    , AddrType(sr.ble_addr_type)
    , EvtType(sr.ble_evt_type)
    , Rssi(sr.rssi)
    , AdvDataLen(sr.adv_data_len)
    , ScanRspLen(sr.scan_rsp_len)
    , EirData(std::to_array(sr.ble_adv))
{
}

BleSpecific::Eir::Eir(const std::array<std::uint8_t, Eir::Size> & data)
    : Data(data)
    , Records(ParseEirRecords(Data))
{
}

std::vector<BleSpecific::Eir::Record>
BleSpecific::Eir::ParseEirRecords(const std::array<std::uint8_t, BleSpecific::Eir::Size> & eir)
{
	std::vector<BleSpecific::Eir::Record> vec;

	int i = 0;
	while (i < eir.size()) {
		// [Length][Type][Data]
		const std::uint8_t length = eir[i];
		if (length == 0 || (i + 2 > eir.size() /* corrupted */)) {
			break;
		}
		vec.emplace_back(static_cast<esp_ble_adv_data_type>(eir[i + 1]),
		                 std::span<const std::uint8_t>(&(eir[i + 2]), length - 1));
		i += length + 1;
	}
	vec.shrink_to_fit();
	return vec;
}

Device::Device(const Gap::Bt::DiscRes & dr)
    : Bda(dr.bda)
    , Data(dr)
{
}

Device::Device(const Gap::Ble::ScanResult & sr)
    : Bda(sr.bda)
    , Data(sr)
{
}

const BrEdrSpecific & Device::GetBrEdr() const
{
	return std::get<BrEdrSpecific>(Data);
}

const BleSpecific & Device::GetBle() const
{
	return std::get<BleSpecific>(Data);
}

std::int8_t Device::GetRssi() const
{
	return IsBle() ? GetBle().Rssi : GetBrEdr().Rssi;
}

bool Device::IsBrEdr() const
{
	return std::holds_alternative<BrEdrSpecific>(Data);
}

bool Device::IsBle() const
{
	return std::holds_alternative<BleSpecific>(Data);
}

}  // namespace Bt

std::string ToString(const Bt::Device & d)
{
	std::stringstream ss;
	// clang-format off
	ss << "{ Bda: \"" << ToString(d.Bda)
	   << ", ";
	if (d.IsBle()) {
		const auto & ble = std::get<Bt::BleSpecific>(d.Data);
		ss << "DevType: \"Ble\", "
		   << "Rssi: " << (int)ble.Rssi
		   << ", AddrType: " << ToString(ble.AddrType)
		   << ", SearchEvt: " << ToString(ble.SearchEvt)
		   << ", AdvDataLen: " << (int)ble.AdvDataLen
		   << ", ScanRspLen: " << (int)ble.ScanRspLen
		   << " }";
	}
	else {
		const auto & bt = std::get<Bt::BrEdrSpecific>(d.Data);
		ss << "DevType: \"BrEdr\", "
		   << ", Rssi: " << (int)bt.Rssi
		   << ", DeviceName: " << bt.DeviceName
		   << ", COD: " << bt.ClassOfDevice
		   << " }";
	}
	// clang-format on
	return ss.str();
}

/*void _ParseRecord(std::span<const uint8_t> data, const esp_ble_adv_data_type recordType)
{
    // Record types' descriptions from: Supplement to the Bluetooth Core Specification

    switch(recordType) {
    default:
        break;
    case ESP_BLE_AD_TYPE_FLAG: // 0x01 - Flags
        break;

    case ESP_BLE_AD_TYPE_16SRV_PART: // 0x02 - Incomplete List of 16-bit Service Class UUIDs
    case ESP_BLE_AD_TYPE_16SRV_CMPL: // 0x03 - Complete List of 16-bit Service Class UUIDs
    case ESP_BLE_AD_TYPE_32SRV_PART: // 0x04 - Incomplete List of 32-bit Service Class UUIDs
    case ESP_BLE_AD_TYPE_32SRV_CMPL: // 0x05 - Complete List of 32-bit Service Class UUIDs

    case ESP_BLE_AD_TYPE_128SRV_PART: // 0x06 - Incomplete List of 128-bit Service Class UUIDs
    case ESP_BLE_AD_TYPE_128SRV_CMPL: // 0x07 - Complete List of 128-bit Service Class UUIDs
        break;
    case ESP_BLE_AD_TYPE_NAME_SHORT: // 0x08 - Shortened Local Name
    case ESP_BLE_AD_TYPE_NAME_CMPL: // 0x09 - Complete Local Name
        break;
    case ESP_BLE_AD_TYPE_TX_PWR: // 0x0A - Tx Power Level
        break;
    case ESP_BLE_AD_TYPE_DEV_CLASS: // 0x0D - Class of Device
    case ESP_BLE_AD_TYPE_SM_TK: // 0x10 - Security Manager TK Value
    case ESP_BLE_AD_TYPE_SM_OOB_FLAG: // 0x11 - Security Manager Out of Band Flags
    case ESP_BLE_AD_TYPE_INT_RANGE: // 0x12 - Peripheral Connection Interval Range
    case ESP_BLE_AD_TYPE_SOL_SRV_UUID: // 0x14 - List of 16-bit Service Solicitation UUIDs
    case ESP_BLE_AD_TYPE_128SOL_SRV_UUID: // 0x15 - List of 128-bit Service Solicitation UUIDs
    case ESP_BLE_AD_TYPE_SERVICE_DATA: // 0x16 - Service Data - 16-bit UUID
    case ESP_BLE_AD_TYPE_PUBLIC_TARGET: // 0x17 - Public Target Address
    case ESP_BLE_AD_TYPE_RANDOM_TARGET: // 0x18 - Random Target Address
    case ESP_BLE_AD_TYPE_APPEARANCE: // 0x19 - Appearance
    case ESP_BLE_AD_TYPE_ADV_INT: // 0x1A - Advertising Interval
    case ESP_BLE_AD_TYPE_LE_DEV_ADDR: // 0x1B - LE Bluetooth Device Address
    case ESP_BLE_AD_TYPE_LE_ROLE: // 0x1C - LE Role
    case ESP_BLE_AD_TYPE_SPAIR_C256: // 0x1D - Simple Pairing Hash C-256
    case ESP_BLE_AD_TYPE_SPAIR_R256: // 0x1E - Simple Pairing Randomizer R-256
    case ESP_BLE_AD_TYPE_32SOL_SRV_UUID: // 0x1F - List of 32-bit Service Solicitation UUIDs
    case ESP_BLE_AD_TYPE_32SERVICE_DATA: // 0x20 - Service Data - 32-bit UUID
    case ESP_BLE_AD_TYPE_128SERVICE_DATA: // 0x21 - Service Data - 128-bit UUID
    case ESP_BLE_AD_TYPE_LE_SECURE_CONFIRM: // 0x22 - LE Secure Connections Confirmation Value
    case ESP_BLE_AD_TYPE_LE_SECURE_RANDOM: // 0x23 - LE Secure Connections Random Value
    case ESP_BLE_AD_TYPE_URI: // 0x24 - URI
    case ESP_BLE_AD_TYPE_INDOOR_POSITION: // 0x25 - Indoor Positioning
    case ESP_BLE_AD_TYPE_TRANS_DISC_DATA: // 0x26 - Transport Discovery Data
    case ESP_BLE_AD_TYPE_LE_SUPPORT_FEATURE: // 0x27 - LE Supported Features
    case ESP_BLE_AD_TYPE_CHAN_MAP_UPDATE: // 0x28 - Channel Map Update Indication
    case ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE: // 0xFF - Manufacturer Specific Data

    }
}*/