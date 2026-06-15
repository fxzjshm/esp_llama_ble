# ESP LLAMA BLE

**BLE HID version of the LLAMA (Low Latency Antenna Module Adapter)**

Replaces the original ESPNOW wireless link with Bluetooth LE HID,
allowing the Alpakka controller to connect directly to a PC without
requiring a USB dongle.

Edited from original esp_llama repo, code mostly by large language models + coding agents.

## Dependencies

```bash
git clone --recursive --branch "v6.0.1" https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh esp32-c2
. ./export.sh
```

## Build

```bash
cd esp_llama_ble
idf.py reconfigure   # Apply sdkconfig.defaults
idf.py build
make binpack
```

## Flash

Flash via RP2040 (DEVICE_LLAMA target):
After 'make binpack', copy build/llama.bin.c to esp_llama/build/
and flash the RP2040 with DEVICE_LLAMA firmware.

In alpakka_firmware:
```bash
DEVICE=llama make -j16
make load
```

Press Home + Up-left button and hold to enter bootsel mode, or use the Web Contol GUI,
then mount the FAT16 partition shown, picotool should copy the uf2 into rp2040 chip.

After rp2040 has flashed ESP32-C2 (LED no longer blinks), build and flash alpakka_v1 firmware:

```bash
DEVICE=alpakka_v1 make -j16
make load
```

## Limitation

* The requested latency is set to 7.5 -- 11.25 ms, however the host may not accept that value.
* May require a re-connection after pairing with the host to get a lower latency
* When switching between hosts (e.g. two different PCs), better to forget the Alpakka BLE device in the new host first, then re-pair in the new host, otherwise strange things may happen. (TODO, tried fixing but no luck yet.)

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
- Standard BLE HID pairing
- No dongle needed — connects directly to PC Bluetooth
- USB protocol sync and WebUSB relay are disabled
