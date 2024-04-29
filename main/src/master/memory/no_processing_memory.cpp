#include "master/memory/no_processing_memory.h"

#include <cassert>

#include <esp_log.h>

namespace
{
/// @brief Logger tag
static const char * TAG = "DevMem";
}  // namespace

namespace Master
{
NoProcessingMemory::NoProcessingMemory(const AppConfig::DeviceMemoryConfig & cfg)
    : IDeviceMemory(cfg)
{
	_serializedData.reserve(MaxSerializedDataSize);
	_scanners.reserve(_cfg.MaxScanners);
}

void NoProcessingMemory::AddScanner(const ScannerInfo & scanner)
{
	if (_scanners.size() >= _cfg.MaxScanners) {
		ESP_LOGW(TAG, "Reached connected scanner limit. Ignoring new scanner.");
		return;
	}
	auto it = std::find_if(_scanners.begin(), _scanners.end(),
	                       [&scanner](const ScannerInfo & s) { return s.Bda == scanner.Bda; });
	if (it == _scanners.end()) {
		// New
		_scanners.push_back(scanner);
	}
	else {
		// Already exists; just update
		it->ConnId = scanner.ConnId;
		it->Service = scanner.Service;
	}
}

void NoProcessingMemory::RemoveScanner(std::uint16_t connId)
{
	auto it = std::find_if(_scanners.begin(), _scanners.end(),
	                       [connId](const ScannerInfo & sc) { return sc.ConnId == connId; });
	if (it == _scanners.end()) {
		return;
	}
	const auto sIdx = std::distance(_scanners.begin(), it);
	_scanners.erase(it);

	// Invalidate any measurements with this scanner
	for (NoProcDeviceMeasurements & meas : _measurements) {
		if (meas.Data[sIdx].IsValid()) {
			meas.Data[sIdx].Invalidate();
			meas.ValidMeasurements--;
		}
	}
}

const ScannerInfo * NoProcessingMemory::GetScanner(std::uint16_t connId) const
{
	auto it = std::find_if(_scanners.begin(), _scanners.end(),
	                       [connId](const ScannerDetail & s) { return s.Info.ConnId == connId; });
	return (it == _scanners.end()) ? nullptr : &*it;
}

void NoProcessingMemory::VisitScanners(const std::function<void(const ScannerInfo &)> & fn)
{
	std::for_each(_scanners.begin(), _scanners.end(), fn);
}

bool NoProcessingMemory::IsConnectedScanner(const Bt::Device & dev) const
{
	// Early simple checks
	if (!dev.IsBle() || dev.GetBle().AddrType != BLE_ADDR_TYPE_PUBLIC
	    || dev.GetBle().EirData.Records.size() != 1) {
		return false;
	}

	return std::find_if(_scanners.begin(), _scanners.end(),
	                    [&dev](const ScannerInfo & s) { return s.Bda == dev.Bda; })
	       != _scanners.end();
}

void NoProcessingMemory::UpdateDistance(const std::uint16_t scannerConnId,
                                        const Core::DeviceDataView::Array & devices)
{
	auto it =
	    std::find_if(_scanners.begin(), _scanners.end(),
	                 [scannerConnId](const ScannerInfo & s) { return s.ConnId == scannerConnId; });
	if (it != _scanners.end()) {
		_UpdateDistance(it, devices);
	}
	else {
		ESP_LOGW(TAG, "Scanner connId %d not found ", scannerConnId);
	}
}

void NoProcessingMemory::UpdateDistance(const Mac & scanner,
                                        const Core::DeviceDataView::Array & devices)
{
	if (auto it = _FindScanner(scanner); it != _scanners.end()) {
		_UpdateDistance(it, devices);
	}
	else {
		ESP_LOGW(TAG, "Scanner %s not found ", ToString(scanner).c_str());
	}
}

std::span<std::uint8_t> NoProcessingMemory::SerializeOutput()
{
	// 4B Timestamp, 1B scanner count (N), 1B measurements Count
	//             6B
	// [Scanner] { MAC }
	///                 6B       N*5B        1B   1B    1B     62B =  71+5*N
	/// [Measurement] { MAC (Timepoint,RSSI) Flag Len EvtType AdvData }

	constexpr std::size_t SingleScannerSize = 6;
	// Resize to max, shrink at the end
	_serializedData.resize(MaxSerializedDataSize);

	std::size_t offset = 6;
	// Scanners
	for (std::size_t i = 0; i < _scanners.size(); i++) {
		std::copy(_scanners[i].Bda.Addr.begin(), _scanners[i].Bda.Addr.end(),
		          _serializedData.data() + offset);
		offset += SingleScannerSize;
	}

	// Measurements
	std::size_t serializedMeasurements = 0;
	for (std::size_t i = 0; i < _measurements.size(); i++) {
		const auto & m = _measurements.at(i);
		if (m.ValidMeasurements == 0) {
			continue;
		}
		serializedMeasurements++;

		// BDA
		std::copy(m.Info.Bda.Addr.begin(), m.Info.Bda.Addr.end(), _serializedData.data() + offset);
		offset += 6;

		// (timestamp, RSSI)
		for (std::size_t j = 0; j < _scanners.size(); j++) {
			const auto & dat = m.Data.at(j);

			const auto time = ToUnix(dat.LastUpdate);
			std::copy_n(reinterpret_cast<const std::uint8_t *>(&time), 4,
			            _serializedData.data() + offset);
			_serializedData[offset + 4] = dat.Rssi;

			offset += 4 + 1;
		}

		// Flags
		_serializedData[offset++] = m.Info.Flags;        // Flags
		_serializedData[offset++] = m.Info.AdvDataSize;  // AdvDataLen
		_serializedData[offset++] = m.Info.EventType;    // EventT

		// AdvData
		std::copy(m.Info.AdvData.begin(), m.Info.AdvData.end(), _serializedData.begin() + offset);
		offset += m.Info.AdvData.size();
	}
	const std::uint32_t now = ToUnix(Clock::now());
	std::copy_n(reinterpret_cast<const std::uint8_t *>(&now), 4, _serializedData.data());
	_serializedData.at(4) = _scanners.size();
	_serializedData.at(5) = serializedMeasurements;

	_serializedData.resize(GetSerializedDataSize(serializedMeasurements, _scanners.size()));

	ESP_LOGI(TAG, "Serialized %d scanners, %d measurements", _scanners.size(),
	         serializedMeasurements);
	return _serializedData;
}

NoProcessingMemory::ScannerIt NoProcessingMemory::_FindScanner(const Mac & mac)
{
	return std::find_if(_scanners.begin(), _scanners.end(),
	                    [&mac](const ScannerInfo & s) { return s.Bda == mac; });
}

NoProcessingMemory::MeasurementIt NoProcessingMemory::_FindMeasurement(const Mac & mac)
{
	return std::find_if(_measurements.begin(), _measurements.end(),
	                    [&mac](const NoProcDeviceMeasurements & s) {
		                    return (s.ValidMeasurements > 0) ? (s.Info.Bda == mac) : false;
	                    });
}

void NoProcessingMemory::_UpdateDistance(ScannerIt sIt, const Core::DeviceDataView::Array & devices)
{
	const std::size_t sIdx = std::distance(_scanners.begin(), sIt);
	for (std::size_t i = 0; i < devices.Size; i++) {
		const Core::DeviceDataView & view = devices[i];

		// We don't care whether it's between a Scanner<->Device or Scanner<->Scanner
		auto it = _FindMeasurement(view.Mac());
		if (it != _measurements.end()) {
			// Update
			if (!it->Data[sIdx].IsValid()) {
				it->ValidMeasurements++;
			}
		}
		else {
			// New - find the oldest one to swap with
			it = std::min_element(_measurements.begin(), _measurements.end(),
			                      [](const NoProcDeviceMeasurements & value,
			                         const NoProcDeviceMeasurements & smallest) {
				                      return value.LastUpdate < smallest.LastUpdate;
			                      });
			assert(it != _measurements.end());

			it->Info.Bda = view.Mac();
			it->Info.Flags = view.Flags();
			it->Info.AdvDataSize = view.AdvDataSize(), it->Info.EventType = view.EventType(),
			std::copy(view.AdvData().begin(), view.AdvData().end(), it->Info.AdvData.begin());

			for (auto & d : it->Data) {
				d.Invalidate();
			}
			it->ValidMeasurements = 1;
		}
		const auto now = Clock::now();
		it->Data[sIdx].Rssi = view.Rssi();
		it->Data[sIdx].LastUpdate = now;
		it->LastUpdate = now;
	}
}

}  // namespace Master
