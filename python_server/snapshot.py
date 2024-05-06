from datetime import datetime
from dataclasses import dataclass
from typing import Self, Optional
import pickle

from common import BleEventType, Mac

@dataclass
class Snapshot:
    """Saved data"""
    @dataclass
    class ScannerData: # (6+3*4)=18B
        mac: Mac
        x: float
        y: float
        z: float
    @dataclass
    class DeviceData: # (6+3*4+2*4+62)=88B; assuming bool=4B
        mac: Mac
        x: float
        y: float
        z: float
        is_ble: bool
        is_public_addr: bool
        event_type: BleEventType
        advertising_data: bytes
    timestamp: datetime
    scanners: list[ScannerData]
    devices: list[DeviceData]

    @staticmethod
    def serialize(snapshots: list[Self]) -> bytes:
        return pickle.dumps(snapshots)

    @staticmethod
    def deserialize(data: bytes) -> list[Self]:
        try:
            out = pickle.loads(data)
            return out
        except Exception as e:
            print(f'Exception while loading ({e})')
        return None

    @staticmethod
    def export(snapshots: list[Self], filename: str):
        with open(filename, "wb") as f:
            pickle.dump(snapshots, f)

    def find(self, mac: Mac) -> Optional[ScannerData | DeviceData]:
        for s in self.scanners:
            if s.mac == mac:
                return s
        for d in self.devices:
            if d.mac == mac:
                return d
        return None
