#include "scanner/device_memory.h"

#include <algorithm>

#include <esp_log.h>

namespace
{
/// @brief Logger tag
static const char * TAG = "DevMem";
}  // namespace

namespace Scanner
{

DeviceInfo::DeviceInfo(std::span<const std::uint8_t, 6> mac,
                       std::int8_t rssi,
                       std::uint8_t flags,
                       esp_ble_evt_type_t eventType,
                       std::span<const std::uint8_t> data)
    : _outData(mac, rssi, flags, eventType, data)
    , _firstUpdate(Clock::now())
    , _lastUpdate(_firstUpdate)
    , _rssi(rssi)
{
}

void DeviceInfo::Update(std::int8_t rssi)
{
	_lastUpdate = Clock::now();
	_rssi.Add(rssi);
	_outData.View.Rssi() = _rssi.Average();
}

void DeviceInfo::Serialize(std::vector<std::uint8_t> & out) const
{
	out.reserve(out.size() + _outData.Data.size());
	std::copy(std::begin(_outData.Data), std::end(_outData.Data), std::back_inserter(out));
}

const Core::DeviceData & DeviceInfo::GetDeviceData() const
{
	return _outData;
}

const DeviceInfo::TimePoint & DeviceInfo::GetLastUpdate() const
{
	return _lastUpdate;
}

/*const DeviceInfo::TimePoint & DeviceInfo::GetUpdateInterval() const
{
    return _updateInterval;
}*/

DeviceInfo::AverageWindow::AverageWindow(std::int8_t rssi)
{
	std::fill(Window.begin(), Window.end(), rssi);
}

void DeviceInfo::AverageWindow::Add(std::int8_t value)
{
	Window[Idx] = value;
	Idx = _NextIdx(Idx);
}

std::int8_t DeviceInfo::AverageWindow::Average() const
{
	std::int16_t sum = 0;
	for (const std::int8_t value : Window) {
		sum += value;
	}
	return static_cast<std::int8_t>(sum / Size);
}

std::uint8_t DeviceInfo::AverageWindow::_NextIdx(std::uint8_t idx)
{
	return idx + 1 == Size ? 0 : idx + 1;
}

DeviceMemory::DeviceMemory(std::size_t sizeLimit)
    : _sizeLimit(sizeLimit)
{
	_devData.reserve(_sizeLimit);
}

void DeviceMemory::AddDevice(const Bt::Device & device)
{
	if (_devData.size() >= _sizeLimit) {
		ESP_LOGI(TAG, "Reached size limit");
		return;
	}

	// Erase stale devices and try to find if this new device already exists.
	DeviceInfo * devFromVec = nullptr;

	const auto now = DeviceInfo::Clock::now();
	std::erase_if(_devData, [&](DeviceInfo & dev) {
		if (std::equal(device.Bda.Addr.begin(), device.Bda.Addr.end(),
		               dev.GetDeviceData().Data.data())) {
			devFromVec = &dev;
			return false;
		}
		return std::chrono::duration_cast<std::chrono::milliseconds>(now - dev.GetLastUpdate())
		           .count()
		       > StaleLimit;
	});

	if (devFromVec) {
		// Found the device, just update it
		devFromVec->Update(device.GetRssi());
		return;
	}
	// Device doesn't exist, attempt to associate
	if (_AssociateDevice(device)) {
		return;
	}
	// Couldn't associate; create new
	// Set flags
	std::uint8_t flags = 0;
	if (device.IsBle()) {
		flags |= Core::FlagMask::IsBle;
		if (device.GetBle().AddrType == BLE_ADDR_TYPE_PUBLIC) {
			flags |= Core::FlagMask::IsAddrTypePublic;
		}
	}

	const std::span<const std::uint8_t, 6> mac{device.Bda.Addr.begin(), 6};
	if (device.IsBle()) {
		// New BLE device
		const auto & ble = device.GetBle();

		std::span<const std::uint8_t, Bt::BleSpecific::Eir::Size> eir{ble.EirData.Data};
		_devData.emplace_back(mac, device.GetRssi(), flags, ble.EvtType, eir);
	}
	else {
		// New BR/EDR device
		// const auto & brEdr = device.GetBrEdr();
		const esp_ble_evt_type_t evt = ESP_BLE_EVT_CONN_ADV;
		// TODO: Could pass some data
		_devData.emplace_back(mac, device.GetRssi(), flags, evt, std::span<const std::uint8_t>());
	}
}

void DeviceMemory::SerializeData(std::vector<std::uint8_t> & out)
{
	RemoveStaleDevices();

	out.clear();
	out.resize(_devData.size() * Core::DeviceDataView::Size);
	for (std::size_t i = 0; i < _devData.size(); i++) {
		const Core::DeviceData & data = _devData[i].GetDeviceData();
		const std::size_t offset = i * Core::DeviceDataView::Size;
		std::copy_n(data.Data.begin(), Core::DeviceDataView::Size, out.data() + offset);
	}
}

void DeviceMemory::RemoveStaleDevices()
{
	// Remove devices not updated for `StaleLimit`
	const auto now = DeviceInfo::Clock::now();

	std::erase_if(_devData, [&](const DeviceInfo & dev) {
		return std::chrono::duration_cast<std::chrono::milliseconds>(now - dev.GetLastUpdate())
		           .count()
		       > StaleLimit;
	});
}

bool DeviceMemory::_AssociateDevice(const Bt::Device & device)
{
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
			// Associated with device - update
			devInfo.Update(device.GetRssi());
			return true;
		}
	}
	return false;
}
}  // namespace Scanner
