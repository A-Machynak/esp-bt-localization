from typing import Self

import requests
import plotly.express as px
from dataclasses import dataclass

@dataclass
class DeviceMinimal:
    mac: list[int]
    x: float | None
    y: float | None
    z: float | None
    rssi: list[int]

@dataclass
class Scanner(DeviceMinimal):
    id: int  # Unique identifier

@dataclass
class Device(DeviceMinimal):
    is_ble: bool
    is_public_addr: bool
    event_type: int
    advertising_data: bytes

    @staticmethod
    def parse(scanner_count: int, b: bytes) -> Self:
        return Device(
            [b[i] for i in range(0, 6)], # MAC
            None, None, None, # X,Y,Z
            [b[i] for i in range(6, 6 + scanner_count)], # RSSIs
            (b[6 + scanner_count] & 0b00000001) != 0, # IsBle
            (b[6 + scanner_count] & 0b00000010) != 0, # IsPublicAddr
            b[8 + scanner_count], # Event type
            b[9 + scanner_count:9 + scanner_count + b[7 + scanner_count]] # AdvData
        )

@dataclass
class Snapshot:
    scanners: list[Scanner]

import random
x = [ random.randint(0, 250) for i in range(0, 72) ]
dev = Device.parse(1, x)
print(dev)

class Memory:
    device_history: list[DeviceMinimal]
    devices: list[Device]
    scanners: list[Scanner]

    def __init__(self) -> None:
        pass


#fig = px.scatter(x=[0, 1, 2, 3, 4], y=[0, 1, 4, 9, 16])
#fig.show()
