#include "core/device_data.h"

#include <esp_log.h>
#include <cstring>

namespace Core
{

DeviceData::DeviceData(std::span<const std::uint8_t> data)
    : Data({})
    , View(Data)
{
	std::copy_n(data.begin(), std::min(DeviceDataView::Size, data.size()), Data.data());
}

DeviceData::DeviceData(std::uint32_t timestamp,
                       std::span<const std::uint8_t, 6> mac,
                       std::int8_t rssi,
                       FlagMask flags,
                       esp_ble_evt_type_t eventType,
                       std::span<const std::uint8_t> advData)
    : Data({})
    , View(Data)
{
	std::copy_n((std::uint8_t*)&timestamp, 4, Data.data() + DeviceDataView::TimepointIdx);
	std::copy_n(mac.data(), mac.size(), Data.data() + DeviceDataView::MacStartIdx);
	Data[DeviceDataView::RssiIdx] = rssi;
	Data[DeviceDataView::FlagsIdx] = flags;
	Data[DeviceDataView::AdvDataSizeIdx] = std::clamp((int)advData.size(), 0, 62);
	Data[DeviceDataView::AdvEventTypeIdx] = eventType;
	std::copy_n(advData.data(), Data[DeviceDataView::AdvDataSizeIdx],
	            Data.data() + DeviceDataView::AdvDataStartIdx);
}

DeviceDataView::DeviceDataView(std::span<std::uint8_t, DeviceDataView::Size> data)
    : Span(data)
{
}

std::uint32_t & DeviceDataView::Timestamp()
{
	return reinterpret_cast<std::uint32_t &>(Span[0]);
}
std::uint32_t DeviceDataView::Timestamp() const
{
	return reinterpret_cast<std::uint32_t &>(Span[0]);
}

std::span<std::uint8_t, 6> DeviceDataView::Mac()
{
	return std::span<std::uint8_t, 6>(Span.begin() + MacStartIdx, Span.begin() + MacEndIdx);
}
std::span<const std::uint8_t, 6> DeviceDataView::Mac() const
{
	return std::span<const std::uint8_t, 6>(Span.begin() + MacStartIdx, Span.begin() + MacEndIdx);
}

std::int8_t & DeviceDataView::Rssi()
{
	return reinterpret_cast<std::int8_t &>(Span[RssiIdx]);
}
std::int8_t DeviceDataView::Rssi() const
{
	return static_cast<std::int8_t>(Span[RssiIdx]);
}

std::uint8_t & DeviceDataView::Flags()
{
	return Span[FlagsIdx];
}
std::uint8_t DeviceDataView::Flags() const
{
	return Span[FlagsIdx];
}

std::uint8_t & DeviceDataView::AdvDataSize()
{
	return Span[AdvDataSizeIdx];
}
std::uint8_t DeviceDataView::AdvDataSize() const
{
	return Span[AdvDataSizeIdx];
}

esp_ble_evt_type_t & DeviceDataView::EventType()
{
	return reinterpret_cast<esp_ble_evt_type_t &>(Span[AdvEventTypeIdx]);
}
esp_ble_evt_type_t DeviceDataView::EventType() const
{
	return static_cast<esp_ble_evt_type_t>(Span[AdvEventTypeIdx]);
}

std::span<std::uint8_t, 62> DeviceDataView::AdvData()
{
	return std::span<std::uint8_t, 62>(Span.begin() + AdvDataStartIdx,
	                                   Span.begin() + AdvDataEndIdx);
}
std::span<const std::uint8_t, 62> DeviceDataView::AdvData() const
{
	return std::span<const std::uint8_t, 62>(Span.begin() + AdvDataStartIdx,
	                                         Span.begin() + AdvDataEndIdx);
}

bool DeviceDataView::IsAddrTypePublic() const
{
	return Span[FlagsIdx] & FlagMask::IsAddrTypePublic;
}

bool DeviceDataView::IsBle() const
{
	return Span[FlagsIdx] & FlagMask::IsBle;
}

DeviceDataView::Array::Array(std::span<std::uint8_t> data)
    : Span(data)
    , Size(data.size() / DeviceDataView::Size)
{
}

DeviceDataView DeviceDataView::Array::operator[](std::size_t i)
{
	assert(i < Size);
	return DeviceDataView(std::span<std::uint8_t, DeviceDataView::Size>(
	    &Span[i * DeviceDataView::Size], DeviceDataView::Size));
}

DeviceDataView DeviceDataView::Array::operator[](std::size_t i) const
{
	assert(i < Size);
	return DeviceDataView(std::span<std::uint8_t, DeviceDataView::Size>(
	    &Span[i * DeviceDataView::Size], DeviceDataView::Size));
}
}  // namespace Core
