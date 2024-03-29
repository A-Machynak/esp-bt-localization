#include "master/device_memory.h"

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

MeasurementData::MeasurementData(std::size_t scannerIdx, std::int8_t rssi)
    : ScannerIdx(scannerIdx)
    , Rssi(rssi)
{
}

DeviceMeasurements::DeviceMeasurements(const Mac & bda, const MeasurementData & firstMeasurement)
    : Bda(bda)
    , Data({firstMeasurement})
    , Position{InvalidPos, InvalidPos, InvalidPos}
{
}

DeviceMemory::DeviceMemory()
{
	_serializedData.reserve(128 * 19);
}

void DeviceMemory::AddScanner(const ScannerInfo & scanner)
{
	// Find if it already exists
	if (auto sc = _scannerMap.find(scanner.Bda); sc != _scannerMap.end()) {
		// Already exists
		ScannerInfo & scInfo = _scanners.at(sc->second);
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
			_deviceMap.erase(scDev);
		}
	}
	ESP_LOGI(TAG, "%d scanners connected", _scanners.size());
	UpdateScannerPositions();
}

void DeviceMemory::UpdateDistance(const std::uint16_t scannerConnId,
                                  std::span<const Mac> device,
                                  std::span<const std::int8_t> rssi)
{
	if (device.size() == 0) {
		return;
	}

	assert(device.size() == rssi.size());
	// Find connection id - we have to iterate through everything
	for (std::size_t i = 0; i < _scanners.size(); i++) {
		if (_scanners[i].ConnId == scannerConnId) {
			_UpdateDistance(i, device, rssi);
			break;
		}
	}
}

void DeviceMemory::UpdateDistance(const Mac & scanner,
                                  std::span<const Mac> device,
                                  std::span<const std::int8_t> rssi)
{
	if (device.size() == 0) {
		return;
	}

	assert(device.size() == rssi.size());
	if (auto sc = _scannerMap.find(scanner); sc != _scannerMap.end()) {
		_UpdateDistance(sc->second, device, rssi);
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
		if (_scanners.at(i).ConnId == connId) {
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
	if (_scannerDistances.Rows() < 2) {
		_scannerDistancesSet = false;
		return nullptr;
	}

	if (_scannerPositions.Rows() != _scanners.size()) {
		_scannerPositions.Reshape(_scanners.size(), Dimensions);
		for (int i = 0; i < _scanners.size(); i++) {
			if (_scannerPositions(i, 0) == 0.0 && _scannerPositions(i, 1) == 0.0
			    && _scannerPositions(i, 2) == 0.0) {
				// we don't care, just don't be 0
				_scannerPositions(i, 0) = (float)(std::rand() & 0xF) * 0.25;
				_scannerPositions(i, 1) = (float)(std::rand() & 0xF) * 0.25;
			}
		}
	}

	const Math::AnchorDistance3D fn(_scannerDistances);
	Math::Minimize(fn, _scannerPositions.Data());

	_scannerDistancesSet = true;

	for (int i = 0; i < _scannerDistances.Rows(); i++) {
		for (int j = i + 1; j < _scannerDistances.Cols(); j++) {
			ESP_LOGI(TAG, "[%d-%d]: %.2f", i, j, _scannerDistances(i, j));
		}
	}

	for (int i = 0; i < _scannerPositions.Rows(); i++) {
		ESP_LOGI(TAG, "Pos: %.2f %.2f %.2f", _scannerPositions(i, 0), _scannerPositions(i, 1),
		         _scannerPositions(i, 2));
	}

	return &_scannerPositions;
}

const std::vector<DeviceMeasurements> & DeviceMemory::UpdateDevicePositions()
{
	if (!_scannerDistancesSet) {
		if (!UpdateScannerPositions()) {  // Maybe user didn't update it?
			return _devices;              // - Nope, can't calculate
		}
	}

	// Calculate the center of the scanners.
	// This will be used as the initial guess
	float xAvg = 0.0, yAvg = 0.0, zAvg = 0.0;
	for (std::size_t i = 0; i < _scanners.size(); i++) {
		xAvg += _scannerPositions(i, 0);
		yAvg += _scannerPositions(i, 1);
		zAvg += _scannerPositions(i, 2);
	}
	xAvg /= _scanners.size();
	yAvg /= _scanners.size();
	zAvg /= _scanners.size();

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
		pos[0] = xAvg;
		pos[1] = yAvg;
		pos[2] = zAvg;

		const Math::PointToAnchors fn(_scannerPositions, tmpDist);
		Math::Minimize(fn, pos);
		ESP_LOGI(TAG, "Dev %.2f %.2f %.2f", pos[0], pos[1], pos[2]);
	}
	return _devices;
}

std::span<std::uint8_t> DeviceMemory::SerializeOutput()
{
	if (_scannerPositions.Rows() != _scanners.size()) {
		return {};  // Probably didn't update yet.
	}
	const std::size_t validDevices =
	    std::accumulate(_devices.begin(), _devices.end(), 0,
	                    [](const std::size_t i, const DeviceMeasurements & dev) {
		                    return dev.IsInvalidPos() ? i : i + 1;
	                    });

	_serializedData.resize((_scanners.size() + validDevices) * DeviceOut::Size);

	std::size_t offset = 0;
	for (std::size_t i = 0; i < _scanners.size(); i++) {
		const auto & scan = _scanners.at(i);

		std::span<std::uint8_t, 19> out(_serializedData.data() + offset, DeviceOut::Size);
		std::span<const std::uint8_t, 6> bda(scan.Bda.Addr);
		std::span<const float, 3> pos(_scannerPositions.Row(i));
		DeviceOut::Serialize(out, bda, pos, true, true, true);

		offset += DeviceOut::Size;
	}
	for (std::size_t i = 0; i < _devices.size(); i++) {
		const auto & dev = _devices.at(i);
		if (dev.IsInvalidPos()) {
			continue;
		}

		std::span<std::uint8_t, 19> out(_serializedData.data() + offset, DeviceOut::Size);
		std::span<const std::uint8_t, 6> bda(dev.Bda.Addr);
		std::span<const float, 3> pos(dev.Position);
		DeviceOut::Serialize(out, bda, pos, false, true, false);  // TODO:FIX

		offset += DeviceOut::Size;
	}
	ESP_LOGI(TAG, "Serialized %d scanners, %d devices", _scanners.size(), _devices.size());

	return _serializedData;
}

const std::vector<ScannerInfo> & DeviceMemory::GetScanners() const
{
	return _scanners;
}

void DeviceMemory::_UpdateDistance(const std::size_t scannerIdx,
                                   std::span<const Mac> devices,
                                   std::span<const std::int8_t> rssis)
{
	for (std::size_t i = 0; i < devices.size(); i++) {
		const Mac & bda = devices[i];
		const std::int8_t rssi = rssis[i];
		// ESP_LOGI(TAG, "UD: %s - %s", ToString(_scanners[scannerIdx].Bda).c_str(),
		//          ToString(bda).c_str());

		if (auto dev = _deviceMap.find(bda); dev != _deviceMap.end()) {
			// Found device
			_UpdateDevice(scannerIdx, dev->second, rssi);
		}
		else if (auto s1 = _scannerMap.find(bda); s1 != _scannerMap.end()) {
			// It's a scanner
			_UpdateScanner(scannerIdx, s1->second, rssi);
		}
		else {
			// Not a device nor a scanner -> new device
			_devices.emplace_back(bda, MeasurementData{scannerIdx, rssi});
			_deviceMap.emplace(bda, _devices.size() - 1);
		}
	}
}

void DeviceMemory::_UpdateDevice(std::size_t scannerIdx, std::size_t devIdx, std::int8_t rssi)
{
	const auto IfScanIdx = [scannerIdx](const MeasurementData & m) -> bool {
		return m.ScannerIdx == scannerIdx;
	};

	// Look up if measurement already exists
	std::vector<MeasurementData> & devMeas = _devices.at(devIdx).Data;
	auto meas = std::find_if(devMeas.begin(), devMeas.end(), IfScanIdx);
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
	_scannerDistancesSet = false;

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
	_scannerMap.erase(_scanners.at(scannerIdx).Bda);
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

}  // namespace Master