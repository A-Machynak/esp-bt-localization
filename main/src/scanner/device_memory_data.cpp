#include "scanner/device_memory_data.h"

#include <chrono>
#include <numeric>

namespace
{
std::uint32_t UnixTimestamp()
{
	return std::chrono::duration_cast<std::chrono::seconds>(
	           Scanner::Clock::now().time_since_epoch())
	    .count();
}
}  // namespace

namespace Scanner
{

DeviceInfo::DeviceInfo(std::span<const std::uint8_t, 6> bda,
                       std::int8_t rssi,
                       Core::FlagMask flags,
                       esp_ble_evt_type_t eventType,
                       std::span<const std::uint8_t> data)
    : _outData(UnixTimestamp(), bda, rssi, flags, eventType, data)
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

void DeviceInfo::Update(std::span<const std::uint8_t, 6> bda, std::int8_t rssi)
{
	std::copy_n(bda.begin(), bda.size(), _outData.View.Mac().data());

	_lastUpdate = Clock::now();
	_rssi.Add(rssi);
	_outData.View.Rssi() = _rssi.Average();
}

void DeviceInfo::Serialize(std::span<std::uint8_t, Core::DeviceDataView::Size> output) const
{
	std::copy_n(_outData.Data.data(), _outData.Data.size(), output.data());
}

const Core::DeviceData & DeviceInfo::GetDeviceData() const
{
	return _outData;
}

const TimePoint & DeviceInfo::GetLastUpdate() const
{
	return _lastUpdate;
}

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
	return std::accumulate(Window.begin(), Window.end(), static_cast<std::int16_t>(0)) / Size;
}

std::uint8_t DeviceInfo::AverageWindow::_NextIdx(std::uint8_t idx)
{
	return ((idx + 1) == Size) ? 0 : (idx + 1);
}

}  // namespace Scanner
