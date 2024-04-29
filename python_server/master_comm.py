"""Communicating with Master. Parsing data. Dataclasses"""

from dataclasses import dataclass
from typing import Optional
from datetime import datetime

import requests
import numpy as np

from common import AdvertisingData, BleEventType, Mac

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

def get_data(ip: str) -> Optional[MasterData]:
    """Get data from Devices endpoint"""
    try:
        response = requests.get(f"http://{ip}/api/devices", timeout=3)
        if response.status_code == 200:
            return parse_response(response.content)
        else:
            print(f"status {response.status_code}")
        return None
    except Exception as e:
        print(f"Exception during GET ({e})")
        return None

def force_advertise(ip: str, mac: Mac):
    """Force a Scanner to an advertising state"""
    msg_type = 4
    data = bytes([ msg_type, *mac.value ])
    _ = requests.post(f"http://{ip}/api/config", data=data, timeout=3)

def parse_response(response_data: bytes) -> MasterData:
    """Parse response from Devices endpoint"""
    scanner_list: list[Mac] = []
    measurement_list: list[MeasurementData] = []

    if len(response_data) < 6:
        print(f"Incorrect response from Master ({response_data})")
        return MasterData(0, [], [])

    now = datetime.timestamp(datetime.now())
    # Timestamp from when the data was sent - we can (circa) find the real timestamps of measurements with this
    ref_timestamp = int.from_bytes(response_data[0:4], byteorder="little")
    scanner_count = response_data[4]
    measurement_count = response_data[5]

    try:
        offset = 6
        # Scanners
        for _ in range(0, scanner_count):
            scanner_list.append(Mac([ int(i) for i in response_data[offset:offset + 6] ]))
            offset += 6

        # Devices
        for _ in range(0, measurement_count):
            # 6B MAC
            mac = Mac([ int(i) for i in response_data[offset:offset + 6] ])
            offset += 6

            measurements: list[MeasurementData.Measurement] = []
            for i in range(0, scanner_count):
                ts = int.from_bytes(response_data[offset:offset + 4], byteorder="little")
                real_ts = now - (ref_timestamp - ts)
                rssi = int(np.int8(response_data[offset + 4]))
                measurements.append(MeasurementData.Measurement(
                    real_ts, rssi, scanner_list[i]
                ))
                offset += 5

            # 1B Flag, 1B Advertising data length, 1B Event type
            flags = response_data[offset]
            offset += 1
            adv_data_len = response_data[offset]
            offset += 1
            event_type =  BleEventType(response_data[offset])
            offset += 1

            # 62B Advertising data (only [adv_data_len]B are valid)
            adv_data = AdvertisingData(response_data[offset:offset+adv_data_len])
            measurement_list.append(MeasurementData(mac, measurements, flags, event_type, adv_data))
            offset += 62
    except Exception as e:
        print(f"Exception during parse_response ({e})")
    return MasterData(now, scanner_list, measurement_list)
