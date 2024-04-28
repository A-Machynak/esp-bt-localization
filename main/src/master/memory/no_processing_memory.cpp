#include "master/memory/no_processing_memory.h"

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
	if (it != _scanners.end()) {
		_scanners.erase(it);
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
}

void NoProcessingMemory::UpdateDistance(const Mac & scanner,
                                        const Core::DeviceDataView::Array & devices)
{
	if (auto it = _FindScanner(scanner); it != _scanners.end()) {
		_UpdateDistance(it, devices);
	}
}

std::span<std::uint8_t> NoProcessingMemory::SerializeOutput()
{
	// 1B scanner count (N), 1B measurements Count
	//             6B
	// [Scanner] { MAC }
	const std::size_t singleScannerSize = 6;
	const std::size_t scannerSize = singleScannerSize * _scanners.size();

	//                    4B         6B          6B      1B     1B      1B         1B     62B
	// [Measurement] { Timepoint, ScannerMAC, DeviceMAC, RSSI, Flag, AdvDataLen, EventT, AdvData }
	const std::size_t singleMeasSize = 82;  // = 82
	const std::size_t devicesSize = singleMeasSize * _measurements.size();

	_serializedData.resize(scannerSize + devicesSize);

	_serializedData.at(0) = _scanners.size();
	_serializedData.at(1) = _measurements.size();

	std::size_t offset = 2;
	// Scanners
	for (std::size_t i = 0; i < _scanners.size(); i++) {
		std::copy(_scanners[i].Bda.Addr.begin(), _scanners[i].Bda.Addr.end(),
		          _serializedData.data() + offset);
		offset += singleScannerSize;
	}

	// Measurements
	for (std::size_t i = 0; i < _measurements.size(); i++) {
		const auto & m = _measurements.at(i);
		// BDA
		std::copy(m.Info.Bda.Addr.begin(), m.Info.Bda.Addr.end(), _serializedData.data() + offset);
		offset += 6;

		// (timestamp, RSSI)
		for (std::size_t j = 0; j < m.Data.size(); j++) {
			constexpr std::size_t rSize = 4 + 1;
			const auto & dat = m.Data.at(j);
			const auto time = ToUnix(dat.LastUpdate);

			std::copy_n(reinterpret_cast<const std::uint8_t *>(&time), 4,
			            _serializedData.data() + offset + (dat.ScannerIdx * rSize));
			_serializedData[offset + (dat.ScannerIdx * rSize) + 4] = dat.Rssi;
		}
		offset += 5 * _scanners.size();

		// Flags
		_serializedData[offset++] = m.Info.Flags;        // Flags
		_serializedData[offset++] = m.Info.AdvDataSize;  // AdvDataLen
		_serializedData[offset++] = m.Info.EventType;    // EventT

		// AdvData
		std::copy(m.Info.AdvData.begin(), m.Info.AdvData.end(), _serializedData.begin() + offset);
		offset += m.Info.AdvData.size();
	}

	_measurements.clear();

	ESP_LOGI(TAG, "Serialized %d scanners, %d measurements", _scanners.size(),
	         _measurements.size());
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
	                    [&mac](const NoProcDeviceMeasurements & s) { return s.Info.Bda == mac; });
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
			auto meas =
			    std::find_if(it->Data.begin(), it->Data.end(),
			                 [sIdx](const MeasurementData & m) { return m.ScannerIdx == sIdx; });
			meas->Rssi = meas->Rssi;
			meas->LastUpdate = Clock::now();
		}
		else {
			// New
			NoProcDeviceMeasurements meas{DeviceInfo{.Bda = view.Mac(),
			                                         .Flags = view.Flags(),
			                                         .AdvDataSize = view.AdvDataSize(),
			                                         .EventType = view.EventType(),
			                                         .AdvData = {}},
			                              {MeasurementData(sIdx, view.Rssi())}};
			std::copy(view.AdvData().begin(), view.AdvData().end(), meas.Info.AdvData.begin());
			_measurements.push_back(meas);
		}
	}
}

}  // namespace Master
