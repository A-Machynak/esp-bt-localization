#include "master/device_memory.h"

#include "core/device_data.h"
#include "master/nvs_utils.h"
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

DeviceMemory::DeviceMemory(const AppConfig::DeviceMemoryConfig & cfg)
    : _cfg(cfg)
{
	_serializedData.reserve(MaximumDevices * DeviceOut::Size);
	_scannerRssis.Reserve(_cfg.MaxScanners * _cfg.MaxScanners);
	_scannerDistances.Reserve(_cfg.MaxScanners * _cfg.MaxScanners);
	_scannerPositions.Reserve(_cfg.MaxScanners * Dimensions);
}

void DeviceMemory::AddScanner(const ScannerInfo & scanner)
{
	if (_scanners.size() >= _cfg.MaxScanners) {
		ESP_LOGW(TAG, "Reached connected scanner limit. Ignoring new scanner.");
		return;
	}

	// Find if it already exists
	if (auto sc = _FindScanner(scanner.Bda); sc != _scanners.end()) {
		// Already exists
		ScannerInfo & scInfo = sc->Info;
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

		_scannerDistances.Reshape(_scanners.size(), _scanners.size());
		_scannerRssis.Reshape(_scanners.size(), _scanners.size());

		if (auto scDev = _FindDevice(scanner.Bda); scDev != _devices.end()) {
			// Already found as a device; erase it
			_devices.erase(scDev);
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
	for (auto it = _scanners.begin(); it != _scanners.end(); it++) {
		if (it->Info.ConnId == scannerConnId) {
			_UpdateDistance(it, device);
			break;
		}
	}
}

void DeviceMemory::UpdateDistance(const Mac & scanner, const Core::DeviceDataView::Array & device)
{
	if (device.Size == 0) {
		return;
	}

	if (auto sc = _FindScanner(scanner); sc != _scanners.end()) {
		_UpdateDistance(sc, device);
	}
}

const ScannerInfo * DeviceMemory::GetScannerToAdvertise() const
{
	for (std::size_t i = 0; i < _scanners.size(); i++) {
		for (std::size_t j = i + 1; j < _scanners.size(); j++) {
			// Scanner RSSI between i-j/j-i missing?
			const ScannerInfo * iInfo = &(_scanners.begin() + i)->Info;
			const ScannerInfo * jInfo = &(_scanners.begin() + j)->Info;
			if (_scannerRssis(i, j) == 0) {
				ESP_LOGI(TAG, "%s should advertise; %s doesn't have measurement",
				         ToString(jInfo->Bda).c_str(), ToString(iInfo->Bda).c_str());
				return jInfo;
			}
			else if (_scannerRssis(j, i) == 0) {
				ESP_LOGI(TAG, "%s should advertise; %s doesn't have measurement",
				         ToString(iInfo->Bda).c_str(), ToString(jInfo->Bda).c_str());
				return iInfo;
			}
		}
	}
	return nullptr;
}

void DeviceMemory::RemoveScanner(std::uint16_t connId)
{
	// Find the scanner
	auto it = std::find_if(_scanners.begin(), _scanners.end(),
	                       [connId](const ScannerDetail & s) { return s.Info.ConnId == connId; });
	if (it != _scanners.end()) {
		_RemoveScanner(it);
	}
}

bool DeviceMemory::IsConnectedScanner(const Bt::Device & dev) const
{
	// Early simple checks
	if (!dev.IsBle() || dev.GetBle().AddrType != BLE_ADDR_TYPE_PUBLIC
	    || dev.GetBle().EirData.Records.size() != 1) {
		return false;
	}

	return std::find_if(_scanners.begin(), _scanners.end(),
	                    [&dev](const ScannerDetail & s) { return s.Info.Bda == dev.Bda; })
	       != _scanners.end();
}

const Math::Matrix<float> * DeviceMemory::UpdateScannerPositions()
{
	if (_cfg.NoPositionCalculation) {
		return nullptr;
	}

	if (_scannerDistances.Rows() < _cfg.MinScanners) {
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
			if ((i != j) && _scannerRssis(i, j) != 0) {
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

	ESP_LOGI(TAG, "Scanners updated; Center (x,y): %.2f %.2f", _scannerCenter[0],
	         _scannerCenter[1]);

	return &_scannerPositions;
}

const std::vector<DeviceMeasurements> & DeviceMemory::UpdateDevicePositions()
{
	_RemoveStaleDevices();

	if (_cfg.NoPositionCalculation) {
		return _devices;
	}

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
		if (meas.Data.size() < _cfg.MinMeasurements) {
			continue;
		}

		// Save distances
		for (auto & m : meas.Data) {
			auto v = Nvs::Cache::Instance().GetValues(meas.Info.Bda.Addr);
			const auto refPathLoss = v.RefPathLoss.value_or(_cfg.DefaultPathLoss);
			const auto envFactor = v.EnvFactor.value_or(_cfg.DefaultEnvFactor);

			tmpDist.at(m.ScannerIdx) = PathLoss::LogDistance(m.Rssi, envFactor, refPathLoss);
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
	if (_cfg.NoPositionCalculation) {
		_SerializeRaw();
	}
	else {
		_SerializeWithPositions();
	}
	return _serializedData;
}

void DeviceMemory::_SerializeWithPositions()
{
	if (_scannerPositions.Rows() != _scanners.size()) {
		return;  // Probably didn't update yet.
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
		const std::span<const float, Dimensions> pos(_scannerPositions.Row(i));
		DeviceOut::Serialize(out, bda, pos, scan.UsedMeasurements, true, true, true);

		offset += DeviceOut::Size;
	}

	// Serialize devices
	std::size_t devicesSerialized = 0;
	for (std::size_t i = 0; i < _devices.size(); i++) {
		const auto & dev = _devices.at(i);
		if (dev.IsInvalidPos()) {
			continue;
		}
		devicesSerialized++;

		const std::span<std::uint8_t, DeviceOut::Size> out(_serializedData.data() + offset,
		                                                   DeviceOut::Size);
		const std::span<const std::uint8_t, 6> bda(dev.Info.Bda.Addr);
		const std::span<const float, 3> pos(dev.Position);

		DeviceOut::Serialize(out, bda, pos, dev.Data.size(), false, dev.Info.IsBle(),
		                     dev.Info.IsAddrTypePublic());

		offset += DeviceOut::Size;
	}
	ESP_LOGI(TAG, "Serialized %d scanners, %d devices", _scanners.size(), devicesSerialized);
}

void DeviceMemory::_SerializeRaw()
{
	// 1B scanner count (N), 1B Device Count
	//             6B    1*N    = (6 + N)
	// [Scanner] { MAC, RSSI[N] }
	const std::size_t singleScannerSize = 6 + _scanners.size();
	const std::size_t scannerSize = singleScannerSize * _scanners.size();

	//             6B    1*N      1B      1B          1B        62B = (71 + N)
	// [Device ] { MAC, RSSI[N], Flags, AdvDataLen, EventT, AdvData }
	const std::size_t singleDeviceSize = 71 + _scanners.size();
	const std::size_t devicesSize = singleDeviceSize * _devices.size();

	_serializedData.resize(scannerSize + devicesSize);

	std::size_t offset = 0;
	// Scanners
	for (std::size_t i = 0; i < _scanners.size(); i++) {
		std::copy(_scanners[i].Info.Bda.Addr.begin(), _scanners[i].Info.Bda.Addr.end(),
		          _serializedData.data() + offset);
		for (std::size_t j = 0; j < _scanners.size(); j++) {
			_serializedData.at(offset + 6 + j) = _scannerRssis(i, j);
		}
		offset += singleScannerSize;
	}

	std::vector<std::int8_t> tmpRssiData;
	tmpRssiData.resize(_scanners.size());
	// Devices
	for (std::size_t i = 0; i < _devices.size(); i++) {
		const auto & dev = _devices.at(i);
		// BDA
		std::copy(dev.Info.Bda.Addr.begin(), dev.Info.Bda.Addr.end(),
		          _serializedData.data() + offset);
		offset += 6;

		// RSSI
		for (std::size_t j = 0; j < dev.Data.size(); j++) {
			const auto & dat = dev.Data.at(j);
			_serializedData[offset + dat.ScannerIdx] = dat.Rssi;
		}
		offset += _scanners.size();

		// Flags
		_serializedData[offset++] = dev.Info.Flags;        // Flags
		_serializedData[offset++] = dev.Info.AdvDataSize;  // AdvDataLen
		_serializedData[offset++] = dev.Info.EventType;    // EventT

		// AdvData
		std::copy(dev.Info.AdvData.begin(), dev.Info.AdvData.end(),
		          _serializedData.begin() + offset);
		offset += dev.Info.AdvData.size();
	}
}

void DeviceMemory::ResetScannerPositions()
{
	_scannerRssis.Fill(0);
	_scannerDistances.Fill(0);
	_scannerPositions.Fill(0.0);
	_scannerPositionsSet = false;
	_devices.clear();
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

DeviceMemory::ScannerIt DeviceMemory::_FindScanner(const Mac & mac)
{
	return std::find_if(_scanners.begin(), _scanners.end(),
	                    [&mac](const ScannerDetail & s) { return s.Info.Bda == mac; });
}

DeviceMemory::DeviceIt DeviceMemory::_FindDevice(const Mac & mac)
{
	return std::find_if(_devices.begin(), _devices.end(),
	                    [&mac](const DeviceMeasurements & s) { return s.Info.Bda == mac; });
}

void DeviceMemory::_AddDevice(DeviceMeasurements device)
{
	if (_devices.size() > MaximumDevices) {
		auto it =
		    std::min_element(_devices.begin(), _devices.end(),
		                     [](const DeviceMeasurements & lhs, const DeviceMeasurements & rhs) {
			                     return lhs.LastUpdate < rhs.LastUpdate;
		                     });
		if (it == _devices.end()) {
			return;
		}
		_devices.erase(it);
	}

	_devices.push_back(std::move(device));
}

void DeviceMemory::_UpdateDistance(ScannerIt sIt, const Core::DeviceDataView::Array & devices)
{
	for (std::size_t i = 0; i < devices.Size; i++) {
		const Core::DeviceDataView & view = devices[i];
		const auto bda = view.Mac();

		// Check scanners first
		if (const auto & s1 = _FindScanner(bda); s1 != _scanners.end()) {
			// It's a scanner
			_UpdateScanner(sIt, s1, view.Rssi());
		}
		else if (const auto & dev = _FindDevice(bda); dev != _devices.end()) {
			// Found device
			_UpdateDevice(sIt, dev, view.Rssi());
		}
		else {
			// Not a device nor a scanner -> new device
			const std::size_t idx = std::distance(_scanners.begin(), sIt);
			_AddDevice(DeviceMeasurements(view, MeasurementData{idx, view.Rssi()}));
		}
	}
}

void DeviceMemory::_UpdateDevice(ScannerIt sIt, DeviceIt devIt, std::int8_t rssi)
{
	// Look up if measurement already exists
	std::vector<MeasurementData> & devMeas = devIt->Data;
	const std::size_t sIdx = std::distance(_scanners.begin(), sIt);
	auto meas = std::find_if(devMeas.begin(), devMeas.end(),
	                         [sIdx](const MeasurementData & m) { return m.ScannerIdx == sIdx; });

	if (meas != devMeas.end()) {
		// Measurement exists, update
		meas->Rssi = (meas->Rssi + rssi) / 2;
	}
	else {
		// First measurement
		devMeas.emplace_back(sIdx, rssi);
	}
}

void DeviceMemory::_UpdateScanner(ScannerIt sc1, ScannerIt sc2, std::int8_t rssi)
{
	const std::size_t sIdx1 = std::distance(_scanners.begin(), sc1);
	const std::size_t sIdx2 = std::distance(_scanners.begin(), sc2);

	_scannerPositionsSet = false;

	// Look up if measurement already exists
	if (_scannerRssis(sIdx1, sIdx2) != 0) {
		// Measurement exists, update
		_scannerRssis(sIdx1, sIdx2) = (_scannerRssis(sIdx1, sIdx2) + rssi) / 2;
	}
	else {
		// First measurement
		_scannerRssis(sIdx1, sIdx2) = rssi;
	}

	const auto & v = Nvs::Cache::Instance().GetValues(_scanners[sIdx1].Info.Bda.Addr);
	const std::int8_t refPathLoss = v.RefPathLoss.value_or(_cfg.DefaultPathLoss);
	const float envFactor = v.EnvFactor.value_or(_cfg.DefaultEnvFactor);
	const std::int8_t rssiVal = _scannerRssis(sIdx1, sIdx2);

	_scannerDistances(sIdx1, sIdx2) = PathLoss::LogDistance(rssiVal, envFactor, refPathLoss);
	if (_scannerRssis(sIdx2, sIdx1) == 0) {  // dist(i, j) == dist(j, i)
		_scannerRssis(sIdx2, sIdx1) = _scannerRssis(sIdx1, sIdx2);
		_scannerDistances(sIdx2, sIdx1) = _scannerDistances(sIdx1, sIdx2);
	}
	ESP_LOGI(TAG, "%s found %s: Rssi: %d, Dist: %.2f, RefPathLoss: %d, EnvFactor: %.2f",
	         ToString(_scanners[sIdx1].Info.Bda.Addr).c_str(),
	         ToString(_scanners[sIdx2].Info.Bda.Addr).c_str(), rssiVal,
	         _scannerDistances(sIdx1, sIdx2), refPathLoss, envFactor);
}

void DeviceMemory::_RemoveScanner(ScannerIt sIt)
{
	const std::size_t sIdx = std::distance(_scanners.begin(), sIt);

	// Remove
	_scanners.erase(sIt);

	// Remove device measurements with this index
	_ResetDeviceMeasurements();

	// Remove from scanner distances - shift and reshape.
	// Shifts everything towards the empty column/row, which gets created by removing this scanner.
	const std::size_t size = _scannerDistances.Rows();  // (Symmetric)
	for (std::size_t i = sIdx + 1; i < size; i++) {
		for (std::size_t j = 0; j < size; j++) {
			_scannerDistances(i, j) = _scannerDistances(i - 1, j);
			_scannerDistances(j, i) = _scannerDistances(j, i - 1);

			_scannerRssis(i, j) = _scannerRssis(i - 1, j);
			_scannerRssis(j, i) = _scannerRssis(j, i - 1);
		}
	}
	_scannerDistances.Reshape(size - 1, size - 1);
	_scannerRssis.Reshape(size - 1, size - 1);

	UpdateScannerPositions();
}

void DeviceMemory::_ResetDeviceMeasurements()
{
	// Remove from devices; just clear everything instead of removing and shifting everything
	for (auto & dev : _devices) {
		dev.Data.clear();
	}
}

void DeviceMemory::_RemoveStaleDevices()
{
	const TimePoint now = Clock::now();  // just calculate it once

	std::erase_if(_devices, [&](const DeviceMeasurements & dev) {
		return (DeltaMs(dev.LastUpdate, now) > _cfg.DeviceStoreTime);
	});
}

void DeviceMemory::_UpdateScannerCenter()
{
	// Calculate the average
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