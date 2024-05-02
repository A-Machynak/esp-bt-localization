#include "scanner/device_memory.h"

#include "core/clock.h"

#include <algorithm>

#include <esp_log.h>

namespace
{
/// @brief Logger tag
static const char * TAG = "DevMem";
}  // namespace

namespace Scanner
{

DeviceMemory::DeviceMemory(const AppConfig::DeviceMemoryConfig & cfg)
    : _cfg(cfg)
{
	_devData.reserve(_cfg.MemorySizeLimit);
}

void DeviceMemory::AddDevice(const Bt::Device & device)
{
	if (device.GetRssi() < _cfg.MinRssi) {
		return;
	}

	// Remove old devices
	RemoveStaleDevices();

	// Try to find this device
	const auto it = std::find_if(_devData.begin(), _devData.end(), [&](const DeviceInfo & dev) {
		return std::equal(device.Bda.Addr.begin(), device.Bda.Addr.end(),
		                  dev.GetDeviceData().View.Mac().begin());
	});

	if (it != _devData.end()) {
		// Found the device, just update it
		it->Update(device.GetRssi());
		return;
	}

	// Device doesn't exist, attempt to associate
	if (_AssociateDevice(device)) {
		return;
	}

	// Couldn't associate, try to make some space for it
	if (_devData.size() >= _cfg.MemorySizeLimit) {
		constexpr std::size_t RssiTolerance = 3;

		// Find a device with the lowest RSSI and random MAC
		auto min = std::min_element(
		    _devData.begin(), _devData.end(),
		    [](const DeviceInfo & value, const DeviceInfo & smallest) {
			    const auto & vView = value.GetDeviceData().View;
			    const auto & sView = smallest.GetDeviceData().View;
			    if (!sView.IsAddrTypePublic()) {
				    // 'smallest' has random MAC
				    if (!vView.IsAddrTypePublic()) {
					    return vView.Rssi() < sView.Rssi();  // ok - both random - compare
				    }
				    return false;  // don't swap smallest with public MAC
			    }  // else 'smallest' has public MAC and we are only looking for random
			    if (vView.IsAddrTypePublic()) {
				    return true;  // random MAC - new smallest
			    }
			    return false;
		    });

		const bool minIsPublic = min->GetDeviceData().View.IsAddrTypePublic();
		const bool minIsCloser =
		    min->GetDeviceData().View.Rssi() > (device.GetRssi() - RssiTolerance);

		if (device.IsBle() && (device.GetBle().AddrType == BLE_ADDR_TYPE_PUBLIC)) {
			// Device has a public MAC; prioritize saving it
			if (minIsPublic && minIsCloser) {
				return;  // Didn't find any device with random MAC + new device has bigger RSSI
			}
		}
		else {  // Device has a random MAC
			if (minIsPublic || minIsCloser) {
				return;
			}
		}
		_devData.erase(min);
	}

	// Found some space; create new
	// Set flags
	std::uint8_t flags = 0;
	if (device.IsBle()) {
		flags |= Core::FlagMask::IsBle;
		if (device.GetBle().AddrType == BLE_ADDR_TYPE_PUBLIC) {
			flags |= Core::FlagMask::IsAddrTypePublic;
		}
	}

	const std::span<const std::uint8_t, 6> mac(device.Bda.Addr);
	if (device.IsBle()) {
		// New BLE device
		const auto & ble = device.GetBle();

		std::span<const std::uint8_t, Bt::BleSpecific::Eir::Size> eir{ble.EirData.Data};
		_devData.emplace_back(mac, device.GetRssi(), static_cast<Core::FlagMask>(flags),
		                      ble.EvtType, eir);
	}
	else {
		// New BR/EDR device
		// const auto & brEdr = device.GetBrEdr();
		const esp_ble_evt_type_t evt = ESP_BLE_EVT_CONN_ADV;
		// TODO: Could pass some data?
		_devData.emplace_back(mac, device.GetRssi(), static_cast<Core::FlagMask>(flags), evt,
		                      std::span<const std::uint8_t>());
	}
}

void DeviceMemory::SerializeData(std::vector<std::uint8_t> & out)
{
	RemoveStaleDevices();

	// Read only up to 512B
	constexpr std::size_t MaxSize = 512;
	constexpr std::size_t MaxDevices = MaxSize / Core::DeviceDataView::Size;

	const std::size_t count = std::min(_devData.size(), MaxDevices);

	out.clear();
	out.resize(count * Core::DeviceDataView::Size);
	for (std::size_t i = 0; i < count; i++) {
		const std::size_t offset = i * Core::DeviceDataView::Size;
		std::span<std::uint8_t, Core::DeviceDataView::Size> span(out.begin() + offset,
		                                                         Core::DeviceDataView::Size);
		_devData.at(i).Serialize(span);
	}
	// Destructive read
	_devData.erase(_devData.begin(), _devData.begin() + count);

	ESP_LOGI(TAG, "Serialized %d devices; %d left to read", count, _devData.size());
}

void DeviceMemory::RemoveStaleDevices()
{
	// Remove devices not updated for `StaleLimit`
	std::erase_if(_devData,
	              [limit = _cfg.StaleLimit, now = Core::Clock::now()](const DeviceInfo & dev) {
		              return Core::DeltaMs(dev.GetLastUpdate(), now) > limit;
	              });
}

bool DeviceMemory::_AssociateDevice(const Bt::Device & device)
{
	if (!_cfg.EnableAssociation) {
		return false;  // Disabled
	}

	// Attempt to associate a device with random address to an already existing one
	if (!device.IsBle()) {
		return false;  // Associate only BLE devices
	}

	const Bt::BleSpecific & ble = device.GetBle();
	if (ble.AddrType == esp_ble_addr_type_t::BLE_ADDR_TYPE_PUBLIC) {
		return false;  // Only random address
	}

	if (ble.AdvDataLen == 0 && ble.ScanRspLen == 0) {
		return false;  // Can't associate with no adv/scan info
	}

	for (auto & devInfo : _devData) {
		const auto & data = devInfo.GetDeviceData();
		if (data.View.AdvDataSize() == ble.AdvDataLen && data.View.EventType() == ble.EvtType
		    && std::equal(data.View.AdvData().begin(), data.View.AdvData().end(),
		                  ble.EirData.Data.begin())) {
			// Associated with device - delete old device and update new one
			devInfo.Update(device.Bda.Addr, device.GetRssi());
			return true;
		}
	}
	return false;
}
}  // namespace Scanner
