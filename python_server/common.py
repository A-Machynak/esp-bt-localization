from dataclasses import dataclass
from enum import Enum
from binascii import unhexlify
from typing import Self
import re

class AdvertisingDataFlag(Enum):
    LE_LIMITED_DISC_MODE = 0x01
    LE_GENERAL_DISC_MODE = 0x02
    BR_EDR_NOT_SUPPORTED = 0x04
    LE_BR_EDR_CONTROLLER = 0x08
    LE_BR_EDR_HOST = 0x10
    LE_ONLY_LIMITED_DISC_MODE = LE_LIMITED_DISC_MODE | BR_EDR_NOT_SUPPORTED
    LE_ONLY_GENERAL_DISC_MODE = LE_GENERAL_DISC_MODE | BR_EDR_NOT_SUPPORTED

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
    PB_ADV = (0x29, "PB-ADV")
    MESH_MESSAGE = (0x2A, "Mesh Message")
    MESH_BEACON = (0x2B, "Mesh Beacon")
    BIGINFO = (0x2C, "BIGInfo")
    BROADCAST_CODE = (0x2D, "Broadcast Code")
    RESOLVABLE_SET_ID = (0x2E, "Resolvable Set Identifier")
    ADVERTISING_INT_LONG = (0x2F, "Advertising Interval - Long")
    BROADCAST_NAME = (0x30, "Broadcast Name")
    ENCRYPTED_ADV_DATA = (0x31, "Encrypted Advertising Data")
    PERIOD_ADV_RESP_TIMING = (0x32, "Periodic Advertising Response Timing Information")
    ELECTR_SHELF_LABEL = (0x34, "Electronic Shelf Label")
    INFO_3D = (0x3D, "3D Information Data")
    MANUFACTURER_SPECIFIC_TYPE = (0xFF, "Manufacturer Specific Data")

class UnknownAdvertisingDataType:
    value: int
    label: str = "Unknown"

    def __init__(self, value: int) -> None:
        self.value = value

@dataclass
class AdvertisingDataRecord:
    type: AdvertisingDataType
    data: bytes

    def __str__(self) -> str:
        if self.type == AdvertisingDataType.FLAG:
            return f"{str(self.type)}: {AdvertisingDataFlag(self.data)}"
        elif self.type == AdvertisingDataType.TX_PWR:
            return f"{str(self.type)}: {int(self.data)}"
        return f"({str(self.type)}: {str(self.data)})"

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
            if int(data[offset]) in [i.value for i in AdvertisingDataType]:
                t = AdvertisingDataType(data[offset])
            else:
                t = UnknownAdvertisingDataType(data[offset])
            records.append(AdvertisingDataRecord(t, data[offset+1:offset+length]))

            offset += length
        return records

class BleEventType(int, Enum):
    def __new__(cls, value: int, label: str):
        obj = int.__new__(cls, value)
        obj._value_ = value
        obj.label = label
        return obj

    CONN_ADV = (0, "Connectable undirected advertising")
    CONN_DIR_ADV = (1, "Connectable directed advertising")
    DISC_ADV = (2, "Scannable undirected advertising")
    NON_CONN_ADV = (3, "Non connectable undirected advertising")
    SCAN_RSP = (4, "Scan response")

MAC_REGEX = re.compile(r'(?:[0-9a-fA-F]:?){12}')
@dataclass
class Mac:
    value: list[int]

    def __eq__(self, value: object) -> bool:
        if isinstance(value, self.__class__):
            return self.value == value.value
        else:
            return False

    def __hash__(self) -> int:
        return hash(tuple(self.value))
    def __str__(self) -> str:
        return ':'.join([format(a, '02x') for a in self.value])

    @staticmethod
    def is_valid(s: str) -> bool:
        return re.match(MAC_REGEX, s) is not None

    @staticmethod
    def from_str(s: str) -> Self:
        bs = unhexlify(s.replace(':', ''))
        return Mac([int(b) for b in bs])
