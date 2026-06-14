# ESP LLAMA BLE

**BLE HID version of the LLAMA (Low Latency Antenna Module Adapter)**

Replaces the original ESPNOW wireless link with Bluetooth LE HID,
allowing the Alpakka controller to connect directly to a PC without
requiring a USB dongle.

## Dependencies
```
git clone --recursive --branch "v6.0.1" https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32-c2
. ./export.sh
```

## Build
```
cd esp_llama_ble
idf.py reconfigure   # Apply sdkconfig.defaults
idf.py build
make binpack
```

## Flash
```
# Direct flash via UART (GPIO20=TX, GPIO19=RX, GND):
idf.py -p /dev/ttyUSB0 flash

# Or flash via RP2040 (DEVICE_LLAMA target):
# After 'make binpack', copy build/llama.bin.c to esp_llama/build/
# and flash the RP2040 with DEVICE_LLAMA firmware.
```

## Monitor
```
idf.py -p /dev/ttyUSB0 monitor
```

## Protocol
The UART protocol with the RP2040 is unchanged from the original LLAMA
firmware. The ESP32 receives framed HID reports via UART (921600 baud,
control-byte delimited) and forwards them as BLE HID notifications.

Supported HID reports:
- Report ID 1: Keyboard (8 bytes)
- Report ID 2: Mouse (7 bytes)
- Report ID 3: Gamepad (16 bytes)

WebUSB/Ctrl protocol messages (AT_WEBUSB) are accepted from the RP2040
but silently dropped — configuration requires a USB cable connection
(wired mode).

## Differences from esp_llama (ESPNOW)
- Uses NimBLE + esp_hidd stack instead of ESP-NOW
- No pairing required — standard BLE HID pairing
- No dongle needed — connects directly to PC Bluetooth
- USB protocol sync and WebUSB relay are disabled
