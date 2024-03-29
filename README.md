# ESP32 Bluetooth Localization

Bluetooth localization based on ESP using RSSI and multilateration techniques and multiple receiving devices.
A single Master device with multiple connected Scanners are used to passively scan for advertising
devices and approximate their positions.

### The principle

A "Scanner" is a device, which waits for an incoming `GATT` connection. Upon connecting, it passively scans
for devices sending BLE Advertisements and saves their BDA (MAC) and RSSI values. These values can then be read
by the connected device using `GATT`.

A "Master" device is used as an aggregator of data saved in Scanners, which listens for BLE Advertisements
from Scanners, connects to them, reads their BDA and RSSI values and attempts to approximate the devices' positions.

<hr>

In order to find the `Scanner <-> Scanner` distance and therefore approximate the position of each Scanner,
the "GATT Scanner Service" provides a way to switch between a "Scanning" mode and "Advertising" mode.

<hr>

Master provides the output in a form of a webpage. An HTTP server is running on port 80, with `/` and `/api/devices` URIs:

| URI | Info |
| --- | ---- |
| `/` | Index page, basic visualization |
| `/api/devices` | Returns an array with 19B/element; see `Visualization` for the format specification |

For Wifi, either AP (creates an access point) or STA (connects to an existing one) mode needs to be selected in the configuration
(with SSID and password). 

## Requirements
- For a Scanner: ESP32 capable of BLE or Bluetooth Classic+BLE (Dual mode)
- For a Master: ESP32 capable of BLE+WiFi
- 4MB Flash

## Configuration

KConfig (`main/Kconfig.projbuild`) is used for configuration.
Use idf `menuconfig` command or edit `sdkconfig.*` to configure it.

## Build, Configure & Flash

Note: Don't forget to do a full clean after changing `sdkconfig` values.

Master
```
idf.py -B build/master -D SDKCONFIG_DEFAULTS="sdkconfig.master" build
idf.py -B build/master menuconfig
idf.py -B build/master flash monitor
```

Scanner
```
idf.py -B build/scanner -D SDKCONFIG_DEFAULTS="sdkconfig.scanner" build
idf.py -B build/scanner menuconfig
idf.py -B build/scanner flash monitor
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

After that, run the `web/zip.py` to embed the js into html (inserted before `</body>`), compress it and output into `compressed.h`. Finally, copy the file's content into `index_page.h`.

The file doesn't need to be minified and can be simply compressed:
```sh
python zip.py visualize.html visualize.js
```
*However*, minifying it will reduce the file size drastically.

<hr>

Custom visualization can be made using the `/api/devices` endpoint.
This endpoint returns an array of elements with 19 bytes per element with the following format (`type:name[size]`):

`uint8:BDA[6]` `float:xyz[3]` `uint8:flags[1]`

... assuming `float` is 4 bytes.
Only the first 3 (lowest) bits in `flags` are used and they represent the following:

|Bit|Info|
|---|----|
|0|Is this device a scanner?|
|1|Is this a BLE device? (Scanners can use BT Classic)|
|2|Is this device's address public? (Can be random)|

### Structure
- `core/` - wrappers over Bluetooth/WiFi API and data common for both Master and Scanner
- `math/` - functions related to minimization (with gradient descent), loss functions to calculate Scanner/device positions and Log Distance Path-Loss model
- `master/` - Master application and related - uses GAP for Scanner discovery, GATT (client) for communication and Wifi+Http server for visualization.
- `scanner/` - Scanner application and related - uses GAP for device discovery and GATT (server) to communicate with another device.

<hr>

- Not having much experience with embedded development/ESP-IDF/Bluetooth and time constraints led to some dubious design choices - unfinished wrappers over C API, improper SRP, ...
- `ToString()` methods convert to `const char *` when possible
