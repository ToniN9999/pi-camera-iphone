# Seeed Studio XIAO ESP32S3 Sense Camera Setup

This guide documents the tested XIAO ESP32S3 Sense + OV3660 BLE camera used with the Pi Camera iPhone app.

## Hardware

- Seeed Studio XIAO ESP32S3 Sense
- OV3660 camera module
- External antenna connected to the board's antenna connector for improved Bluetooth range

## Arduino IDE setup

1. Install Arduino IDE.
2. Add the Espressif ESP32 boards package using this Boards Manager URL:

   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`

3. Install **esp32 by Espressif Systems**, version **3.3.13**.
4. Select board **XIAO_ESP32S3** and the correct serial port.
5. In **Tools → PSRAM**, select **OPI PSRAM**. This setting is essential; without it the camera may fail to initialise or allocate frame buffers.
6. Install **NimBLE-Arduino** by h2zero.
7. Open `XIAO_Camera_BLE.ino`, compile and upload.

## Firmware configuration

The checked-in firmware is configured for the OV3660 sensor and this BLE identity:

- Advertised name: `XIAO-CAMERA`
- BLE service and characteristic UUIDs: identical to the Raspberry Pi and iPhone app
- Camera frame size: QVGA (320 × 240)
- JPEG capture
- Notification chunk size: 500 bytes
- Acknowledgement window: 2,000 bytes
- ACK timeout: 15 seconds
- Transfer timeout: 30 seconds

The firmware flushes one buffered camera frame before taking the requested photograph. This avoids returning the previous image when successive captures are requested.

## iPhone app compatibility

The iPhone app selects its acknowledgement window based on the connected camera name:

- Raspberry Pi: 40,000 bytes
- XIAO (`XIAO-CAMERA`): 2,000 bytes

Do not change the advertised XIAO name without also updating the camera-specific selection in `PiCamera/CameraBluetoothManager.swift`.

## Serial monitor checks

Set the Serial Monitor to **115200 baud**. On startup, the expected messages include:

- `Camera initialised successfully!`
- `Sensor PID: 0x3660`
- `OV3660 ready.`
- `BLE advertising started`
- `Advertising as XIAO-CAMERA`

During a capture, the sketch reports the JPEG size and dimensions. During transfer it reports acknowledgement progress and transfer completion.

## Troubleshooting

### Camera initialisation or frame-buffer allocation fails

Check **Tools → PSRAM → OPI PSRAM**, then upload again.

### Camera appears to return the previous photograph

Use the checked-in frame-flush implementation in `capturePhoto()`. It discards one buffered frame, waits 150 ms, and then captures the fresh frame.

### Bluetooth range is poor

Check that the external antenna is connected correctly. The tested external antenna improved the observed RSSI substantially compared with the onboard antenna arrangement.

## Tested project settings

- ESP32 Arduino core: 3.3.13
- Board: XIAO_ESP32S3
- PSRAM: OPI PSRAM
- BLE library: NimBLE-Arduino by h2zero
- Camera sensor PID: 0x3660 (OV3660)
