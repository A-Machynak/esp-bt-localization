# https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/kconfig.html

# Tag Specific
CONFIG_TAG=y

# C++
CONFIG_COMPILER_CXX_EXCEPTIONS=y

# Bluetooth
CONFIG_BT_ENABLED=y
CONFIG_BT_BLE_ENABLED=y
CONFIG_BLUEDROID_ENABLED=y

CONFIG_BT_BLE_50_FEATURES_SUPPORTED=y  # Might throw undefined reference to esp_ble_X if not defined for some targets
CONFIG_BT_BLE_42_FEATURES_SUPPORTED=y  # Might throw undefined reference to esp_ble_X if not defined for some targets

CONFIG_BTDM_CTRL_MODE_BLE_ONLY=y # BLE only

# Flash Size
CONFIG_ESPTOOLPY_FLASHSIZE="4MB"
CONFIG_ESPTOOLPY_FLASHSIZE_4MB=y

# Partition
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"
CONFIG_PARTITION_TABLE_FILENAME="partitions.csv"
CONFIG_PARTITION_TABLE_OFFSET=0x8000
