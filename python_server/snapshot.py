from datetime import datetime
from dataclasses import dataclass
from typing import Self
from pathlib import Path
import pickle

from common import Mac

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
    class DeviceData: # (6+3*4+62)=80B
        mac: Mac
        x: float
        y: float
        z: float
        advertising_data: bytes
    timestamp: datetime
    scanners: list[ScannerData]
    devices: list[DeviceData]

    @staticmethod
    def serialize(snapshots: list[Self]) -> bytes:
        return pickle.dumps(snapshots)

    @staticmethod
    def load(data: bytes) -> list[Self]:
        try:
            out = pickle.loads(data)
            print(out)
            return out
        except Exception as e:
            print(f'Exception while loading ({e})')
        return None
