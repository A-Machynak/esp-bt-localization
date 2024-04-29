from typing import Self, Optional
from dataclasses import dataclass, field
from enum import Enum
from datetime import datetime

import numpy as np

from utils import log_distance

class AdvertisingDataType(int, Enum):
    def __new__(cls, value: int, label: str):
        obj = int.__new__(cls, value)
        obj._value_ = value
        obj.label = label
        return obj

    FLAG = (0x01, "Flags")
    SRV16_PART = (0x02, "Incomplete List of 16-bit Service Class UUIDs")
    SRV16_CMPL = (0x03, "Complete List of 16-bit Service Class UUIDs")
    SRV32_PART = (0x04, "Incomplete List of 32-bit Service Class UUIDs")
    SRV32_CMPL = (0x05, "Complete List of 32-bit Service Class UUIDs")
    SRV128_PART = (0x06, "Incomplete List of 128-bit Service Class UUIDs")
    SRV128_CMPL = (0x07, "Complete List of 128-bit Service Class UUIDs")
    NAME_SHORT = (0x08, "Shortened Local Name")
    NAME_CMPL = (0x09, "Complete Local Name")
    TX_PWR = (0x0A, "Tx Power Level")
    DEV_CLASS = (0x0D, "Class of Device")
    SM_TK = (0x10, "Security Manager TK Value")
    SM_OOB_FLAG = (0x11, "Security Manager Out of Band Flags")
    INT_RANGE = (0x12, "Peripheral Connection Interval Range")
    SOL_SRV_UUID = (0x14, "List of 16-bit Service Solicitation UUIDs")
    SOL128_SRV_UUID = (0x15, "List of 128-bit Service Solicitation UUIDs")
    SERVICE_DATA = (0x16, "Service Data - 16-bit UUID")
    PUBLIC_TARGET = (0x17, "Public Target Address")
    RANDOM_TARGET = (0x18, "Random Target Address")
    APPEARANCE = (0x19, "Appearance")
    ADV_INT = (0x1A, "Advertising Interval")
    LE_DEV_ADDR = (0x1B, "LE Bluetooth Device Address")
    LE_ROLE = (0x1C, "LE Role")
    SPAIR_C256 = (0x1D, "Simple Pairing Hash C-256")
    SPAIR_R256 = (0x1E, "Simple Pairing Randomizer R-256")
    SOL_SRV32_UUID = (0x1F, "List of 32-bit Service Solicitation UUIDs")
    SERVICE_DATA32 = (0x20, "Service Data - 32-bit UUID")
    SERVICE_DATA128 = (0x21, "Service Data - 128-bit UUID")
    LE_SECURE_CONFIRM = (0x22, "LE Secure Connections Confirmation Value")
    LE_SECURE_RANDOM = (0x23, "LE Secure Connections Random Value")
    URI = (0x24, "URI")
    INDOOR_POSITION = (0x25, "Indoor Positioning")
    TRANS_DISC_DATA = (0x26, "Transport Discovery Data")
    LE_SUPPORT_FEATURE = (0x27, "LE Supported Features")
    TYPE_CHAN_MAP_UPDATE = (0x28, "Channel Map Update Indication")
    MANUFACTURER_SPECIFIC_TYPE = (0xFF, "Manufacturer Specific Data")

@dataclass
class AdvertisingDataRecord:
    type: AdvertisingDataType
    data: bytes

@dataclass
class AdvertisingData:
    records: list[AdvertisingDataRecord]

    def __init__(self, data: bytes):
        self.records = self.parse_records(data)

    @staticmethod
    def parse_records(data: bytes) -> list[AdvertisingDataRecord]:
        records = []
        offset = 0
        while offset < len(data):
            # 0 - Length
            length = data[offset]
            if length <= 0:
                break
            offset += 1

            # 1 - Type
            # <2,N-1> - Data
            records.append(AdvertisingDataRecord(
                AdvertisingDataType(data[offset]),
                data[offset+1:offset+length]
            ))
            offset += length
        return records

class BleEventType(Enum):
    CONN_ADV = 0     # Connectable undirected advertising
    CONN_DIR_ADV = 1 # Connectable directed advertising
    DISC_ADV = 2     # Scannable undirected advertising
    NON_CONN_ADV = 3 # Non connectable undirected advertising
    SCAN_RSP = 4     # Scan response

@dataclass
class Mac:
    value: list[int]

    def __hash__(self) -> int:
        return hash(tuple(self.value))
    def __str__(self) -> str:
        return ':'.join([hex(a)[2:] for a in self.value])

@dataclass
class MeasurementData:
    """Measurement data received from Master"""

    @dataclass
    class Measurement:
        timestamp: datetime
        rssi: int
        scanner_mac: Mac

        def __init__(self, time: int, rssi: int, scanner_mac: Mac) -> None:
            self.timestamp = datetime.fromtimestamp(time)
            self.rssi = rssi
            self.scanner_mac = scanner_mac

        def is_valid(self) -> bool:
            return int(self.rssi) != 0

    mac: Mac
    measurements: list[Measurement]
    flags: int
    event_type: BleEventType
    adv_data: AdvertisingData

@dataclass
class MasterData:
    """Data unit received from master"""
    timestamp: datetime
    scanners: list[Mac]
    measurements: list[MeasurementData]

    def __init__(self, time: int, scanners: list[Mac], measurements: list[MeasurementData]) -> None:
        self.timestamp = datetime.fromtimestamp(time)
        self.scanners = scanners
        self.measurements = measurements

@dataclass
class DeviceBase:
    """Internal representation of common scanner/device data"""

    @dataclass
    class Data:
        rssi: int
        last_update: datetime
        def is_valid(self) -> bool:
            return int(self.rssi) != 0

    mac: Mac
    x: float | None = None
    y: float | None = None
    z: float | None = None
    data: dict[Mac, Data] = field(default_factory=dict)

    def __init__(self, mac: Mac, measurements: list[MeasurementData.Measurement]):
        self.mac = mac
        self.data = {}
        for m in measurements:
            self.data[m.scanner_mac] = DeviceBase.Data(m.rssi, m.timestamp)

    def update(self, measurements: list[MeasurementData.Measurement]):
        for m in measurements:
            if m.scanner_mac in self.data and self.data[m.scanner_mac].is_valid():
                # Measurement exists
                if self.data[m.scanner_mac].last_update < m.timestamp:
                    # Moving average
                    self.data[m.scanner_mac].rssi = (self.data[m.scanner_mac].rssi + m.rssi) / 2
                    self.data[m.scanner_mac].last_update = m.timestamp
            else:
                # New
                self.data[m.scanner_mac] = DeviceBase.Data(m.rssi, m.timestamp)

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

class Memory:
    scanner_dict: dict[Mac, Scanner] = {}
    scanner_indices: dict[Mac, int] = {}
    device_dict: dict[Mac, Device] = {}
    device_history: dict[Mac, DeviceBase] = {}
    last_idx: int = 0

    scanner_positions: np.array = None

    def __init__(self) -> None:
        pass

    def update(self, new_data: MasterData):
        # Early check. This should fail most of the time
        if self._scanners_changed_check(new_data):
            # Add any new scanners
            for new_sc in new_data.scanners:
                if new_sc not in self.scanner_dict:
                    self.scanner_dict[new_sc] = Scanner(new_sc, [])
                if new_sc not in self.scanner_indices:
                    self.scanner_indices[new_sc] = self.last_idx
                    self.last_idx += 1

        # Update all RSSI
        for meas in new_data.measurements:
            if meas.mac in self.scanner_dict:
                print(meas)
                self.scanner_dict[meas.mac].update(meas.measurements)
            elif meas.mac in self.device_dict:
                self.device_dict[meas.mac].update(meas.measurements)
            else:
                # New device
                self.device_dict[meas.mac] = Device(meas.mac, meas.measurements, meas.flags & 0b1,
                                                    meas.flags & 0b10, meas.event_type, meas.adv_data)

    def _scanners_changed_check(self, new_data: MasterData) -> bool:
        for sc in new_data.scanners:
            if sc not in self.scanner_dict:
                return True
        return False
    
    def get_scanner_to_advertise(self) -> Optional[Mac]:
        for mac1, scanner1 in self.scanner_dict.items():
            for mac2, scanner2 in self.scanner_dict.items():
                if (mac1 != mac2) and ((mac2 not in scanner1.data) or (not scanner1.data[mac2].is_valid())):
                    return mac2
        return None

    def scanner_rssi_matrix(self):
        mat = np.zeros(shape=(self.last_idx, self.last_idx))
        for mac1, scanner in self.scanner_dict.items():
            for mac2, dat in scanner.data.items():
                if dat.is_valid():
                    i1 = self.scanner_indices[mac1]
                    i2 = self.scanner_indices[mac2]
                    if mat[i1, i2] == 0:
                        mat[i1, i2] = dat.rssi
                    else:
                        mat[i1, i2] = (mat[i1, i2] + dat.rssi) / 2
                    mat[i2, i1] = mat[i1, i2]

        return mat
    
    def update_scanner_positions(self):
        from sklearn.manifold import MDS
        mat = self.scanner_rssi_matrix()
        dist_mat = np.vectorize(lambda x: log_distance(x))(mat)

        if (self.scanner_positions is not None) and (self.scanner_positions.shape != dist_mat.shape):
            self.scanner_positions.resize(dist_mat.shape)

        n_init = 4 if self.scanner_positions is None else 1
        # MDS
        embedding = MDS(n_components=2, normalized_stress='auto', dissimilarity='precomputed', n_init=n_init)
        self.scanner_positions = embedding.fit_transform(dist_mat, init=self.scanner_positions)


@dataclass
class Snapshot:
    scanners: list[DeviceBase]
