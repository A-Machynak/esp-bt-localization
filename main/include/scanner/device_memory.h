#pragma once

#include <chrono>
#include <cstdint>
#include <span>
#include <vector>

#include "core/device_data.h"
#include "core/wrapper/device.h"

namespace Scanner
{

/// @brief Device stale limit
constexpr std::int64_t StaleLimit = 30'000;

/// @brief Default amount of device limit
constexpr std::size_t DefaultMaxDevices = 128;

/// @brief Calculate memory used by devices when serialized
/// @param devices how many devices
/// @return how many bytes of memory will be used
constexpr std::size_t DeviceMemoryByteSize(std::size_t devices = DefaultMaxDevices)
{
	return devices * Core::DeviceDataView::Size;
}

class DeviceInfo
{
public:
	using Clock = std::chrono::system_clock;
	using TimePoint = std::chrono::time_point<std::chrono::system_clock>;

	DeviceInfo(std::span<const std::uint8_t, 6> mac,
	           std::int8_t rssi,
	           std::uint8_t flags,
	           esp_ble_evt_type_t eventType,
	           std::span<const std::uint8_t> data);

	void Update(std::int8_t rssi);

	void Serialize(std::vector<std::uint8_t> & out) const;

	const Core::DeviceData & GetDeviceData() const;

	const TimePoint & GetLastUpdate() const;

	// const TimePoint & GetUpdateInterval() const;

private:
	/// @brief Average of the last `Size` values
	class AverageWindow
	{
	public:
		AverageWindow(std::int8_t value);

		void Add(std::int8_t value);

		std::int8_t Average() const;

	private:
		constexpr static std::uint8_t Size = 10;
		std::array<std::int8_t, Size> Window;
		std::uint8_t Idx{0};

		static std::uint8_t _NextIdx(std::uint8_t idx);
	};

	Core::DeviceData _outData;  ///< Raw output data
	TimePoint _firstUpdate;     ///< First update
	TimePoint _lastUpdate;      ///< Last RSSI update
	// std::chrono::milliseconds _updateInterval;  ///< Last update interval
	AverageWindow _rssi;  ///< Average window for rssi
};

class DeviceMemory
{
public:
	DeviceMemory(std::size_t sizeLimit = DefaultMaxDevices);

	void AddDevice(const Bt::Device & device);

	void SerializeData(std::vector<std::uint8_t> & out);

	void RemoveStaleDevices();

private:
	const std::size_t _sizeLimit;

	/// @brief Device data
	std::vector<DeviceInfo> _devData;

	bool _AssociateDevice(const Bt::Device & device);
};

}  // namespace Scanner
