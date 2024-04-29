# ESP32 Bluetooth Localization

Bluetooth localization based on ESP using RSSI and multilateration techniques and multiple receiving devices.
A single Master device with multiple connected Scanners are used to passively scan for advertising
devices and approximate their positions.

This project was developed as part of a bachelor's thesis "Bluetooth Based Localization" at [BUT FIT](https://www.fit.vut.cz/.en).

### The principle

A "Scanner" is a device, which waits for an incoming `GATT` connection. Upon connecting, it passively scans
for devices sending BLE Advertisements and saves their BDA (MAC) and RSSI values. These values can then be read
by the connected device using `GATT`.

A "Master" device is used as an aggregator of data saved in Scanners, which listens for BLE Advertisements
from Scanners, connects to them, reads their BDA and RSSI values and attempts to approximate the devices' positions.

A "Tag" is a device, which keeps sending BLE Advertisements.

<hr>

In order to find the `Scanner <-> Scanner` distance and therefore approximate the position of each Scanner,
the "GATT Scanner Service" provides a way to switch between a "Scanning" mode and "Advertising" mode.

<hr>

Master provides the output in a form of a webpage. An HTTP server is running on port 80 (`192.168.4.1` for AP).

For Wifi, either AP (creates an access point) or STA (connects to an existing one) mode needs to be selected in the configuration
(with SSID and password). 

## Requirements
- ESP-IDF v5.2
- For a Scanner or Tag: ESP32 capable of BLE or Bluetooth Classic+BLE (Dual mode)
- For a Master: ESP32 capable of BLE+WiFi
- 4MB Flash

### Compatible devices

The project has been tested on the following devices:

- ESP32-WROOM-32 - Master, Scanner (BT+BLE)
	- Deemed unsuitable - high path loss variance, reference path loss at 50 to 70
- ESP32-C3(-MINI-1) - Master, Scanner (BLE)
	- More suitable - much smaller path loss variance, reference path loss at 36 to 39

## Configuration

KConfig (`main/Kconfig.projbuild`) is used for configuration.
Use idf `menuconfig` command or edit `sdkconfig.*` to configure it.

### Reference path loss/environment factor

Depending on the device, a different reference path loss might be used.
This value represents the path loss at a 1 meter distance and it is usually around 40 to 60.

Depending on the environment, a different environment factor might be used.
This value represents the 

Together, these 2 are the parameters for a Log Distance Path Loss function used to calculate the distances.

## Build, Configure & Flash

For vscode, use `esp_idf_project_configuration.json` - `CTRL+SHIFT+P` -> `ESP-IDF: Select project configuration`.

Or directly with `idf.py`:

Master
```
idf.py -B build/master -D SDKCONFIG_DEFAULTS="config/sdkconfig.master" menuconfig
idf.py -B build/master -D SDKCONFIG_DEFAULTS="config/sdkconfig.master" build flash monitor
```

Scanner
```
idf.py -B build/scanner -D SDKCONFIG_DEFAULTS="config/sdkconfig.scanner" menuconfig
idf.py -B build/scanner -D SDKCONFIG_DEFAULTS="config/sdkconfig.scanner" build flash monitor
```

Tag
```
idf.py -B build/tag -D SDKCONFIG_DEFAULTS="config/sdkconfig.tag" menuconfig
idf.py -B build/tag -D SDKCONFIG_DEFAULTS="config/sdkconfig.tag" build flash monitor
```

## Implementation notes

### Visualization

Files `web/visualize.html` and `web/visualize.js` contain HTML+JS code used as an index page by Master device.
For minimal memory footprint, the file is minified, compressed and stored as a byte array in `main/include/master/http/index_page.h`.

If some changes are made to the `visualize.html` or `visualize.js` file, the files *should be* minified and *need to be* compressed again.
To minify it, [google-closure-compiler](https://github.com/google/closure-compiler) and [html-minifer](https://github.com/kangax/html-minifier) can be used:
```sh
npm i -g google-closure-compiler
npm i -g html-minifier
```
```
google-closure-compiler -O ADVANCED --js web/visualize.js --js_output_file web/visualize.min.js
```
```
html-minifier --collapse-whitespace --remove-comments --remove-optional-tags --remove-redundant-attributes --remove-script-type-attributes --remove-tag-whitespace web/visualize.html -o web/visualize.min.html
```

After that, run the `web/zip.py` to embed the js into html (it'll be inserted before `</body>` or at the end, if not found), compress it and output into `compressed.h`.
Alternatively, the python script can be used to run the minifiers automatically and compress everything at once (`-a` flag).
Finally, copy the file's content into `index_page.h`.

The file doesn't need to be minified and can be simply compressed:
```sh
python zip.py --html_min_file visualize.html --js_min_file visualize.js
```
*However*, minifying it will reduce the file size drastically.

<hr>

Custom visualization can be made using the `/api/devices` endpoint.
Sending GET request on this endpoint returns an array of elements with 20 bytes per element.

`GET /api/devices`

| Type/Size[B] | Name          |
| ------------ | ------------- |
| uint8/6      | BDA           |
| float/3      | (x, y, z)     |
| uint8/1      | Scanner count |
| uint8/1      | Flags         |

... assuming `float` is 4 bytes.
`scannerCount` represents the amount of scanner measurements that were used to approximate this device's position.
For `flags`, only the first 3 (lowest) bits are used and they represent the following:

| Bit | Info                                                |
| --- | --------------------------------------------------- |
| 0   | Is this device a scanner?                           |
| 1   | Is this a BLE device? (Scanners can use BT Classic) |
| 2   | Is this device's address public? (Can be random)    |

<hr>

POST requests expect raw bytes in the format `[Type0][Data0][Type1][Data1]...`, where type is one of {`0`, `1`, `2`}:

`POST /api/config`

| Type | Name                       | Expected data                         |
| ---- | -------------------------- | ------------------------------------- |
| 0    | System message             | 1B - type of message                  |
| 1    | Set reference RSSI         | 6B (MAC) + 1B (RSSI, `int8`)          |
| 2    | Set environment factor     | 6B (MAC) + 4B (EnvFactor, `float`)    |
| 3    | Map MAC to a name (Unused) | 6B (MAC) + up to 16B (name, `string`) |
| 4    | Force Scanner to advertise | 6B (MAC) - Scanner MAC                |

`System message types`
| Type | Name            | Data | Description                                                             |
| ---- | --------------- | ---- | ----------------------------------------------------------------------- |
| 0    | Restart         | None | Restarts the ESP                                                        |
| 1    | Reset Scanners  | None | Resets scanner positions                                                |
| 2    | Switch to AP    | None | Switches WiFi to AP mode (with SSID/password from menuconfig) (Unused)  |
| 3    | Switch to STA   | None | Switches WiFi to STA mode (with SSID/password from menuconfig) (Unused) |

### Custom processing and visualization

Instead of calculating the positions on the device, you can use your own application to process and visualize the data.
To do that, set the `menuconfig` field `Master -> Algorithm -> No position calculation` to `On`.
At that point, the `GET /api/devices` endpoint sends data in the following format:

| Bytes           | Name              | Description                           |
| --------------- | ----------------- | ------------------------------------- |
| 4               | Timestamp         | Unix timestamp when the data was sent |
| 1               | Scanner count (N) | How many scanners are sent            |
| 1               | Device count (M)  | How many devices are sent             |
| N*size(Scanner) | Array of scanners | Array with `Scanner` types (below)    |
| M*size(Device)  | Array of devices  | Array with `Device` types (below)     |

TODO (Device/Scanner format)

### Structure
- `core/` - wrappers over Bluetooth/WiFi API and data common for both Master and Scanner
- `math/` - functions related to minimization (with gradient descent), loss functions to calculate Scanner/device positions and Log Distance Path-Loss model
- `master/` - Master application and related - uses GAP for Scanner discovery, GATT (client) for communication and Wifi+Http server for visualization.
- `scanner/` - Scanner application and related - uses GAP for device discovery and GATT (server) to communicate with another device.

<hr>

- Not having much experience with embedded development/ESP-IDF/Bluetooth and time constraints led to some dubious design choices - unfinished wrappers over C API, improper SRP, ...
- Some classes are initialized with `Init()` method instead of a constructor. This is to allow these classes to be static - if, for example, a class initializes Bluetooth Controller in its constructor, it cannot be statically initialized.
- `ToString()` methods convert to `const char *` when possible
