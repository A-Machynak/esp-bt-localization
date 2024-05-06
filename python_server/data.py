from typing import Optional
from dataclasses import dataclass, field
from datetime import datetime
import random

import numpy as np
from sklearn.manifold import MDS
from sklearn.linear_model import LinearRegression

from algorithm import minimize, log_distance, DEFAULT_ENV_FACTOR, DEFAULT_REF_PATH_LOSS
from common import BleEventType, Mac
from master_comm import MasterData, MeasurementData
from snapshot import Snapshot

@dataclass
class DeviceBase:
    """Internal representation of common scanner/device data"""

    @dataclass
    class Data:
        rssi: int
        last_update: datetime
        update_count: int # Update counter
        def is_valid(self) -> bool:
            return int(self.rssi) != 0

    mac: Mac
    x: float | None = None
    y: float | None = None
    z: float | None = None
    data: dict[Mac, Data] = field(default_factory=dict)
    env_factor: float = DEFAULT_ENV_FACTOR
    ref_path_loss: int = DEFAULT_REF_PATH_LOSS

    def __init__(self, mac: Mac, measurements: list[MeasurementData.Measurement]):
        self.mac = mac
        self.data = {}
        for m in measurements:
            self.data[m.scanner_mac] = DeviceBase.Data(m.rssi, m.timestamp, 1)

    def has_position(self) -> bool:
        return not ((self.x is None) and (self.y is None) and (self.z is None))

    def invalidate_position(self):
        self.x = None
        self.y = None
        self.z = None

    def update(self, measurements: list[MeasurementData.Measurement]):
        for m in measurements:
            if m.scanner_mac in self.data and self.data[m.scanner_mac].is_valid():
                # Measurement exists
                if self.data[m.scanner_mac].last_update < m.timestamp:
                    # Moving average
                    self.data[m.scanner_mac].rssi = (self.data[m.scanner_mac].rssi + m.rssi) / 2
                    self.data[m.scanner_mac].last_update = m.timestamp
                    self.data[m.scanner_mac].update_count += 1
            else:
                # New
                self.data[m.scanner_mac] = DeviceBase.Data(m.rssi, m.timestamp, 1)

@dataclass
class Scanner(DeviceBase):
    """Internal representation of a scanner"""
    is_active: bool = True

    def __init__(self, mac: Mac, measurements: list[MeasurementData.Measurement]):
        super().__init__(mac, measurements)
        self.is_active = True

@dataclass
class Device(DeviceBase):
    """Internal representation of a device"""
    is_ble: bool = False
    is_public_addr: bool = False
    event_type: BleEventType = BleEventType.CONN_ADV
    advertising_data: bytes = field(default_factory=list)

    def __init__(self, mac: Mac, measurements: list[MeasurementData.Measurement],
                 is_ble: bool, is_public_addr: bool, event_type: BleEventType,
                 advertising_data: bytes):
        super().__init__(mac, measurements)
        self.is_ble = is_ble
        self.is_public_addr = is_public_addr
        self.event_type = event_type
        self.advertising_data = advertising_data

    def update_position(self, scanners: dict[Mac, Scanner], min_distances: int = 3):
        scanner_positions = []
        distances = []
        # Find scanners and distances
        for mac, v in self.data.items():
            if v.is_valid() and scanners[mac].is_active and scanners[mac].has_position():
                distances.append(log_distance(v.rssi, self.env_factor, self.ref_path_loss))
                scanner_positions.append([scanners[mac].x, scanners[mac].y, scanners[mac].z])

        if len(distances) < min_distances:
            return # Can't update
        guess = None if not self.has_position() else [self.x, self.y, self.z]
        self.x, self.y, self.z = minimize(guess, distances, scanner_positions)

class Memory:
    scanner_dict: dict[Mac, Scanner] = {}
    scanner_indices: dict[Mac, int] = {}
    device_dict: dict[Mac, Device] = {}
    snapshots: list[Snapshot] = []
    snapshot_limit: int = 16384
    last_idx: int = 0
    recalculate_scanners: bool = True
    use_2d: bool = False # Set Z to 0.0 for scanners' MDS

    def update(self, new_data: MasterData):
        # Early check. This should fail most of the time
        if self._scanners_changed_check(new_data):
            # Add any new scanners
            for new_sc in new_data.scanners:
                if new_sc not in self.scanner_dict:
                    self.scanner_dict[new_sc] = Scanner(new_sc, [])
                else:
                    self.scanner_dict[new_sc].is_active = True
                if new_sc not in self.scanner_indices:
                    self.scanner_indices[new_sc] = self.last_idx
                    self.last_idx += 1
            # Set old scanners as inactive
            for mac, sc in self.scanner_dict.items():
                if mac not in new_data.scanners:
                    sc.is_active = False

        # Update all RSSI
        for meas in new_data.measurements:
            if meas.mac in self.scanner_dict:
                self.recalculate_scanners = True
                self.scanner_dict[meas.mac].update(meas.measurements)
            elif meas.mac in self.device_dict:
                self.device_dict[meas.mac].update(meas.measurements)
            else:
                # New device
                self.device_dict[meas.mac] = Device(meas.mac, meas.measurements, (meas.flags & 0b1) == 0b1,
                                                    (meas.flags & 0b10) == 0b10, meas.event_type, meas.adv_data)

    def _scanners_changed_check(self, new_data: MasterData) -> bool:
        for sc in new_data.scanners:
            if sc not in self.scanner_dict:
                return True
        return False

    def get_scanner_to_advertise(self) -> Optional[Mac]:
        to_advertise = []
        for mac1, scanner1 in self.scanner_dict.items():
            for mac2, scanner2 in self.scanner_dict.items():
                if (mac1 != mac2) and ((mac2 not in scanner1.data) or (not scanner1.data[mac2].is_valid())):
                    to_advertise.append(mac1)
        if len(to_advertise) == 0:
            return None
        return to_advertise[random.randint(0, len(to_advertise) - 1)]

    def _scanner_dist_matrix(self):
        mat = np.full((self.last_idx, self.last_idx), 1e-9)
        for mac1, scanner in self.scanner_dict.items():
            for mac2, dat in scanner.data.items():
                if dat.is_valid():
                    i1 = self.scanner_indices[mac1]
                    i2 = self.scanner_indices[mac2]
                    dist = log_distance(dat.rssi, self.scanner_dict[mac2].env_factor, self.scanner_dict[mac2].ref_path_loss)
                    mat[i1, i2] = dist
        for i in range(0, self.last_idx):
            for j in range(i + 1, self.last_idx):
                mat[i, j] = mat[j, i] = ((mat[i,j] + mat[j,i]) / 2.0)
        return mat

    def reset_scanner_positions(self):
        for mac, sc in self.scanner_dict.items():
            sc.invalidate_position()
            self.recalculate_scanners = True

    def reset_scanner_distances(self):
        for mac, sc in self.scanner_dict.items():
            sc.invalidate_position()
            sc.data.clear()
            self.recalculate_scanners = True

    def update_scanner_positions(self):
        if not self.recalculate_scanners:
            return
        self.recalculate_scanners = False

        dist_mat = self._scanner_dist_matrix()
        positions = np.zeros(shape=(len(self.scanner_dict), 3))
        no_pos = True
        for mac, sc in self.scanner_dict.items():
            idx = self.scanner_indices[mac]
            if sc.has_position():
                no_pos = False
                positions[idx][0] = sc.x
                positions[idx][1] = sc.y
                positions[idx][2] = 0.0 if self.use_2d else sc.z
            elif self.use_2d:
                # Set to random manually
                positions[idx][0] = random.random() * 2.0 - 2.0
                positions[idx][1] = random.random() * 2.0 - 2.0
                positions[idx][2] = 0.0

        n_init = 1
        if no_pos and not self.use_2d:
            # Let MDS generate random positions
            n_init = 4
            positions = None
        # MDS
        embedding = MDS(n_components=3, normalized_stress='auto', dissimilarity='precomputed', n_init=n_init)
        positions = embedding.fit_transform(dist_mat, init=positions)
        """
        # The scanners are usually around the same height. Output positions are quite off though.
        # We can find the regression plane between these points, get the rotation matrix which would rotate
        # it towards its normal and rotate the points using it.
        X = positions[:, :2]
        y = positions[:, 2]
        model = LinearRegression()
        model.fit(X, y)

        # z = ax + by + c
        a, b = model.coef_
        #c = model.intercept_
        normal = np.array([a, b, -1.0])
        normal /= np.linalg.norm(normal)
        up = np.array([0,0,1]) # 'up' vector

        from scipy.spatial.transform import Rotation

        ax = np.cross(normal, up)
        angle = np.arccos(np.dot(normal, up))
        R = Rotation.from_rotvec(angle * ax).as_matrix()
        print(f'before:\n{positions}\nafter:\n{np.dot(R, positions.T).T}')
        positions = np.dot(R, positions.T).T
        # This sucks
        """
        # Update
        for i in range(0, len(positions)):
            for mac, idx in self.scanner_indices.items():
                if i == idx:
                    self.scanner_dict[mac].x = positions[i][0]
                    self.scanner_dict[mac].y = positions[i][1]
                    self.scanner_dict[mac].z = positions[i][2]

    def update_device_positions(self):
        for _, dev in self.device_dict.items():
            dev.update_position(self.scanner_dict)

    def remove_old_data(self, max_since_last_update: float = 60.0):
        now = datetime.now()
        devices_to_pop = []
        for mac1, dev in self.device_dict.items():
            measurements_to_pop = []
            for mac2, m in dev.data.items():
                if (now - m.last_update).seconds > max_since_last_update:
                    measurements_to_pop.append(mac2)
            for meas_mac in measurements_to_pop:
                dev.data.pop(meas_mac)
            if len(dev.data) == 0:
                devices_to_pop.append(mac1)
        for dev_mac in devices_to_pop:
                self.device_dict.pop(dev_mac)

    def create_snapshot(self):
        time = datetime.now()
        scanner_list: list[Snapshot.ScannerData] = []
        device_list: list[Snapshot.DeviceData] = []

        for mac, scanner in self.scanner_dict.items():
            if scanner.has_position():
                scanner_list.append(Snapshot.ScannerData(mac, scanner.x, scanner.y, scanner.z))

        for mac, device in self.device_dict.items():
            if device.has_position():
                device_list.append(Snapshot.DeviceData(device.mac, device.x, device.y, device.z,
                                                       device.is_ble, device.is_public_addr, device.event_type,
                                                       device.advertising_data))

        if len(self.snapshots) > self.snapshot_limit:
            self.snapshots.pop(0)
        self.snapshots.append(Snapshot(time, scanner_list, device_list))

    def find(self, mac: Mac) -> Optional[Scanner | Device]:
        """Find a device/scanner based on MAC"""
        for sc_mac, data in self.scanner_dict.items():
            if sc_mac == mac:
                return data
        for dev_mac, data in self.device_dict.items():
            if dev_mac == mac:
                return data
        return None
