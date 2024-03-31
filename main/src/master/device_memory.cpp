#include "master/device_memory.h"

#include "core/device_data.h"
#include "math/minimizer/functions/anchor_distance.h"
#include "math/minimizer/functions/point_to_anchors.h"
#include "math/minimizer/gradient_minimizer.h"
#include "math/path_loss/log_distance.h"

#include <esp_log.h>

#include <numeric>

namespace
{
/// @brief Logger tag
static const char * TAG = "DevMem";
}  // namespace

namespace Master
{

DeviceMemory::DeviceMemory()
{
	_serializedData.reserve(128 * 19);
}

void DeviceMemory::AddScanner(const ScannerInfo & scanner)
{
	// Find if it already exists
	if (auto sc = _scannerMap.find(scanner.Bda); sc != _scannerMap.end()) {
		// Already exists
		ScannerInfo & scInfo = _scanners.at(sc->second).Info;
		if (scInfo.ConnId == scanner.ConnId) {
			return;  // Duplicate call?
		}
		// Just update
		scInfo.ConnId = scanner.ConnId;
		scInfo.Service = scanner.Service;
	}
	else {
		// New scanner
		_scanners.push_back(scanner);
		const std::size_t newIdx = _scanners.size() - 1;
		_scannerMap.emplace(scanner.Bda, newIdx);
		_scannerDistances.Reshape(_scanners.size(), _scanners.size());

		auto scDev = _deviceMap.find(scanner.Bda);
		if (scDev != _deviceMap.end()) {
			// Already found as a device
			for (auto & meas : _devices[scDev->second].Data) {
				_UpdateScanner(meas.ScannerIdx, newIdx, meas.Rssi);
			}
			_devices.erase(_devices.begin() + scDev->second);
			_deviceMap.erase(scDev);
		}
	}
	ESP_LOGI(TAG, "%d scanners connected", _scanners.size());
	UpdateScannerPositions();
}

void DeviceMemory::UpdateDistance(const std::uint16_t scannerConnId,
                                  const Core::DeviceDataView::Array & device)
{
	if (device.Size == 0) {
		return;
	}

	// Find connection id - we have to iterate through everything
	for (std::size_t i = 0; i < _scanners.size(); i++) {
		if (_scanners.at(i).Info.ConnId == scannerConnId) {
			_UpdateDistance(i, device);
			break;
		}
	}
}

void DeviceMemory::UpdateDistance(const Mac & scanner, const Core::DeviceDataView::Array & device)
{
	if (device.Size == 0) {
		return;
	}

	if (auto sc = _scannerMap.find(scanner); sc != _scannerMap.end()) {
		_UpdateDistance(sc->second, device);
	}
}

std::size_t DeviceMemory::GetScannerIdxToAdvertise() const
{
	for (std::size_t i = 0; i < _scanners.size(); i++) {
		for (std::size_t j = i + 1; j < _scanners.size(); j++) {
			if (_scannerDistances(i, j) == 0.0) {
				// Scanner distance between i-j is missing; return at least one
				return i;
			}
		}
	}
	return InvalidScannerIdx;
}

void DeviceMemory::RemoveScanner(std::uint16_t connId)
{
	// Find the scanner
	for (std::size_t i = 0; i < _scanners.size(); i++) {
		if (_scanners.at(i).Info.ConnId == connId) {
			ESP_LOGI(TAG, "Removing scanner %s", ToString(_scanners.at(i).Info.Bda).c_str());
			_RemoveScanner(connId);
			break;
		}
	}
}

std::size_t DeviceMemory::GetConnectedScannerIdx(const Bt::Device & dev) const
{
	// Early simple checks
	if (!dev.IsBle() || dev.GetBle().AddrType != BLE_ADDR_TYPE_PUBLIC
	    || dev.GetBle().EirData.Records.size() != 1) {
		return InvalidScannerIdx;
	}

	auto it = _scannerMap.find(dev.Bda);
	return (it != _scannerMap.end()) ? it->second : InvalidScannerIdx;
}

bool DeviceMemory::ShouldAdvertise(ScannerIdx idx)
{
	if (idx >= _scanners.size()) {
		return false;
	}

	// Go through each scanner and check if it's missing this scanner's bda
	for (std::size_t i = 0; i < _scanners.size(); i++) {
		if (i != idx && _scannerDistances(i, idx) == 0.0) {
			return true;
		}
	}
	return false;
}

const Math::Matrix<float> * DeviceMemory::UpdateScannerPositions()
{
	if (_scannerDistances.Rows() < MinimumScanners) {
		_scannerPositionsSet = false;
		return nullptr;
	}

	// Update size
	if (_scannerPositions.Rows() != _scanners.size()) {
		_scannerPositions.Reshape(_scanners.size(), Dimensions);

		for (std::size_t i = 0; i < _scanners.size(); i++) {
			const auto row = _scannerPositions.Row(i);
			if (std::all_of(row.begin(), row.end(), [](const float v) { return v == 0.0; })) {
				// Set random x,y positions for new scanners
				_scannerPositions(i, 0) = (float)(std::rand() & 0xF) * 0.25;
				_scannerPositions(i, 1) = (float)(std::rand() & 0xF) * 0.25;
			}
		}
	}

	// Find how many scanner measurements are available for each one
	for (std::size_t i = 0; i < _scanners.size(); i++) {
		auto & scanner = _scanners.at(i);
		scanner.UsedMeasurements = 0;
		for (std::size_t j = 0; i < _scanners.size(); i++) {
			if ((i != j) && _scannerDistances(i, j) != 0.0) {
				scanner.UsedMeasurements++;
			}
		}
	}

	// Calculate new positions
	const Math::AnchorDistance3D fn(_scannerDistances);
	Math::Minimize(fn, _scannerPositions.Data());
	_scannerPositionsSet = true;

	// Recalculate scanner center
	_UpdateScannerCenter();

	return &_scannerPositions;
}

const std::vector<DeviceMeasurements> & DeviceMemory::UpdateDevicePositions()
{
	if (!_scannerPositionsSet) {
		if (!UpdateScannerPositions()) {  // Maybe user didn't update it?
			return _devices;              // - Nope, can't calculate
		}
	}

	// Distances from point to each scanner
	std::vector<float> tmpDist;
	tmpDist.resize(_scanners.size());

	for (std::size_t i = 0; i < _devices.size(); i++) {
		DeviceMeasurements & meas = _devices.at(i);
		if (meas.Data.size() < MinimumMeasurements) {
			continue;
		}

		// Save distances
		for (auto & m : meas.Data) {
			tmpDist.at(m.ScannerIdx) = PathLoss::LogDistance(m.Rssi);
		}

		std::span pos = meas.Position;
		for (std::size_t i = 0; i < pos.size(); i++) {
			pos[i] = _scannerCenter[i];
		}

		const Math::PointToAnchors fn(_scannerPositions, tmpDist);
		Math::Minimize(fn, pos);
	}
	return _devices;
}

std::span<std::uint8_t> DeviceMemory::SerializeOutput()
{
	if (_scannerPositions.Rows() != _scanners.size()) {
		return {};  // Probably didn't update yet.
	}

	// Count how many devices have valid positions
	const std::size_t validDevices =
	    std::accumulate(_devices.begin(), _devices.end(), 0,
	                    [](const std::size_t i, const DeviceMeasurements & dev) {
		                    return dev.IsInvalidPos() ? i : i + 1;
	                    });

	_serializedData.resize((_scanners.size() + validDevices) * DeviceOut::Size);

	// Serialize scanners
	std::size_t offset = 0;
	for (std::size_t i = 0; i < _scannerPositions.Rows(); i++) {
		const auto & scan = _scanners.at(i);

		const std::span<std::uint8_t, DeviceOut::Size> out(_serializedData.data() + offset,
		                                                   DeviceOut::Size);
		const std::span<const std::uint8_t, 6> bda(scan.Info.Bda.Addr);
		const std::span<const float, 3> pos(_scannerPositions.Row(i));
		DeviceOut::Serialize(out, bda, pos, scan.UsedMeasurements, true, true, true);

		offset += DeviceOut::Size;
	}

	// Serialize devices
	for (std::size_t i = 0; i < _devices.size(); i++) {
		const auto & dev = _devices.at(i);
		if (dev.IsInvalidPos()) {
			continue;
		}

		const std::span<std::uint8_t, DeviceOut::Size> out(_serializedData.data() + offset,
		                                                   DeviceOut::Size);
		const std::span<const std::uint8_t, 6> bda(dev.Info.Bda.Addr);
		const std::span<const float, 3> pos(dev.Position);

		DeviceOut::Serialize(out, bda, pos, dev.Data.size(), false, dev.Info.IsBle,
		                     dev.Info.IsAddrTypePublic);

		offset += DeviceOut::Size;
	}
	ESP_LOGI(TAG, "Serialized %d scanners, %d devices", _scanners.size(), _devices.size());

	return _serializedData;
}

const ScannerDetail * DeviceMemory::GetScanner(std::uint16_t connId) const
{
	auto it = std::find_if(_scanners.begin(), _scanners.end(),
	                       [connId](const ScannerDetail & s) { return s.Info.ConnId == connId; });
	return (it == _scanners.end()) ? nullptr : &*it;
}

const std::vector<ScannerDetail> & DeviceMemory::GetScanners() const
{
	return _scanners;
}

void DeviceMemory::_UpdateDistance(const std::size_t scannerIdx,
                                   const Core::DeviceDataView::Array & devices)
{
	for (std::size_t i = 0; i < devices.Size; i++) {
		const Core::DeviceDataView & view = devices[i];
		const auto bda = view.Mac();

		// Check scanners first
		if (const auto & s1 = _scannerMap.find(bda); s1 != _scannerMap.end()) {
			// It's a scanner
			_UpdateScanner(scannerIdx, s1->second, view.Rssi());
		}
		else if (const auto & dev = _deviceMap.find(bda); dev != _deviceMap.end()) {
			// Found device
			_UpdateDevice(scannerIdx, dev->second, view.Rssi());
		}
		else {
			// Not a device nor a scanner -> new device
			_devices.emplace_back(bda, view.IsBle(), view.IsAddrTypePublic(),
			                      MeasurementData{scannerIdx, view.Rssi()});
			_deviceMap.emplace(bda, _devices.size() - 1);
		}
	}
}

void DeviceMemory::_UpdateDevice(std::size_t scannerIdx, std::size_t devIdx, std::int8_t rssi)
{
	// Look up if measurement already exists
	std::vector<MeasurementData> & devMeas = _devices.at(devIdx).Data;
	auto meas =
	    std::find_if(devMeas.begin(), devMeas.end(), [scannerIdx](const MeasurementData & m) {
		    return m.ScannerIdx == scannerIdx;
	    });
	if (meas != devMeas.end()) {
		// Measurement exists, update
		meas->Rssi = (meas->Rssi + rssi) / 2;
	}
	else {
		// First measurement
		devMeas.emplace_back(scannerIdx, rssi);
	}
}

void DeviceMemory::_UpdateScanner(std::size_t sIdx1, std::size_t sIdx2, std::int8_t rssi)
{
	const float dist = PathLoss::LogDistance(rssi);
	_scannerPositionsSet = false;

	// Look up if measurement already exists
	if (_scannerDistances(sIdx1, sIdx2) != 0.0) {
		// Measurement exists, update
		_scannerDistances(sIdx1, sIdx2) = (_scannerDistances(sIdx1, sIdx2) + dist) / 2;
	}
	else {
		// First measurement
		_scannerDistances(sIdx1, sIdx2) = dist;
	}
	_scannerDistances(sIdx2, sIdx1) = dist;  // Symmetric
	ESP_LOGI(TAG, "Scanner distance [%d-%d]: %d (= %.2f)", sIdx1, sIdx2, rssi, dist);
}

void DeviceMemory::_RemoveScanner(std::size_t scannerIdx)
{
	assert(scannerIdx < _scanners.size());

	// Remove
	_scannerMap.erase(_scanners.at(scannerIdx).Info.Bda);
	_scanners.erase(_scanners.begin() + scannerIdx);

	// Remove device measurements with this index
	_RemoveDeviceMeasurements(scannerIdx);

	// Remove from scanner distances - shift and reshape
	const std::size_t size = _scannerDistances.Rows();  // (Symmetric)
	for (std::size_t i = scannerIdx + 1; i < size; i++) {
		for (std::size_t j = 0; j < size; j++) {
			_scannerDistances(i, j) = _scannerDistances(j, i) = _scannerDistances(i - 1, j);
		}
	}
	_scannerDistances.Reshape(size - 1, size - 1);

	UpdateScannerPositions();
}

void DeviceMemory::_RemoveDeviceMeasurements(std::size_t scannerIdx)
{
	// Remove from devices
	for (const auto & [bda, idx] : _deviceMap) {
		std::erase_if(_devices[idx].Data, [scannerIdx](const MeasurementData & meas) {
			return meas.ScannerIdx == scannerIdx;
		});
	}
}

void DeviceMemory::_UpdateScannerCenter()
{
	std::fill(_scannerCenter.begin(), _scannerCenter.end(), 0.0);
	for (std::size_t i = 0; i < _scannerPositions.Rows(); i++) {
		for (std::size_t dim = 0; dim < _scannerCenter.size(); dim++) {
			_scannerCenter.at(dim) += _scannerPositions(i, dim);
		}
	}
	std::for_each(_scannerCenter.begin(), _scannerCenter.end(),
	              [dim = _scannerPositions.Rows()](float & v) { v /= dim; });
}

}  // namespace Master