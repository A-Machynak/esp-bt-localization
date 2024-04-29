import time
import requests
import numpy as np
import pandas as pd
from datetime import datetime
from typing import Optional

from data import AdvertisingData, BleEventType, Device, DeviceBase, Mac, MasterData, MeasurementData, Memory
from utils import log_distance

import dash
from dash import Dash, html, dcc, Input, Output, State, callback

from sklearn.manifold import MDS

App = None
Ip = None

def start(app_name: str, ip: str):
    global App, Ip

    Ip = ip
    #App = dash.Dash(app_name)
    #App.layout = html.Div([
    #    dcc.Input(id='in-master-ip', type='text', placeholder='Master IPv4', value=Ip),
    #    html.Button('Set', id='submit-master-ip', n_clicks=0),
    #])
    #App.run(debug=True)
    update_data_loop()

def update_data_loop():
    mem = Memory()
    while True:
        data = get_data()
        if data:
            mem.update(data)

            # Check advertising
            sc = mem.get_scanner_to_advertise()
            if sc:
                log_info(f"{sc} should advertise")
                force_advertise(sc)

            # MDS
            mem.update_scanner_positions()
            print(mem.scanner_positions)
        time.sleep(5)

@callback(
        Output('in-master-ip', 'value'),
        Input('submit-master-ip', 'n_clicks'),
        State('in-master-ip', 'value'),
        prevent_initial_call=True
)
def set_ip(n_clicks: int, value: str):
    global Ip
    Ip = value
    log_info(f"Updated ip: '{Ip}'")

def get_data() -> Optional[MasterData]:
    try:
        response = requests.get(f"http://{Ip}/api/devices", timeout=3)
        if response.status_code == 200:
            return parse_response(response.content)
        else:
            log_info(f"status {response.status_code}")
        return None
    except Exception as e:
        log_info(f"Exception during GET ({e})")
        return None

def force_advertise(mac: Mac):
    """Force a Scanner to an advertising state"""
    msg_type = 4
    data = bytes([ msg_type, *mac.value ])
    _ = requests.post(f"http://{Ip}/api/config", data=data, timeout=3)

def log_info(msg: str):
    if App:
        App.logger.info(msg)
    else:
        print(msg)

def parse_response(response_data: bytes) -> MasterData:
    scanner_list: list[Mac] = []
    measurement_list: list[MeasurementData] = []

    if len(response_data) < 6:
        log_info(f"Incorrect response from Master ({response_data})")
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
        log_info(f"Exception during parse_response ({e})")
    return MasterData(now, scanner_list, measurement_list)
