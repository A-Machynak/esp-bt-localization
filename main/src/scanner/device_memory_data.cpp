#include "scanner/device_memory_data.h"

#include <numeric>

namespace Scanner
{

DeviceInfo::DeviceInfo(std::span<const std::uint8_t, 6> bda,
                       std::int8_t rssi,
                       Core::FlagMask flags,
                       esp_ble_evt_type_t eventType,
                       std::span<const std::uint8_t> data)
    : _outData(Core::ToUnix(Core::Clock::now()), bda, rssi, flags, eventType, data)
    , _firstUpdate(Core::Clock::now())
    , _lastUpdate(_firstUpdate)
    , _rssi(rssi)
{
}

void DeviceInfo::Update(std::int8_t rssi)
{
	_lastUpdate = Core::Clock::now();
	_rssi.Add(rssi);
	_outData.View.Rssi() = _rssi.Average();
	_outData.View.Timestamp() = Core::ToUnix(_lastUpdate);
}

void DeviceInfo::Update(std::span<const std::uint8_t, 6> bda, std::int8_t rssi)
{
	assert(bda.size() == _outData.View.Mac().size());
	std::copy(bda.begin(), bda.end(), _outData.View.Mac().begin());
	Update(rssi);
}

void DeviceInfo::Serialize(std::span<std::uint8_t, Core::DeviceDataView::Size> output) const
{
	assert(_outData.Data.size() == output.size());
	std::copy(_outData.Data.begin(), _outData.Data.end(), output.begin());
}

const Core::DeviceData & DeviceInfo::GetDeviceData() const
{
	return _outData;
}

const Core::TimePoint & DeviceInfo::GetLastUpdate() const
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
