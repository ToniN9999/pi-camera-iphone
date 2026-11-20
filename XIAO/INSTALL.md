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
7. Open `XIAO_Camera_BLE.ino`, compile and upload. Keep `config.h` in the same sketch folder; Arduino needs both files.

## Firmware configuration

Device-specific settings are kept in **`config.h`**, separate from the main firmware. To give each physical XIAO a unique Bluetooth name, edit only this file:

```cpp
#define CAMERA_NAME "XIAO-CAMERA-01"
```

For another unit, use a different name, such as `XIAO-CAMERA-02`. Keep the `XIAO-` prefix so the iPhone app recognises it as a XIAO and selects the correct acknowledgement window. Do not edit the main `XIAO_Camera_BLE.ino` sketch just to change a device name.

The same file also contains the transfer and camera settings:

- BLE service and characteristic UUIDs: identical to the Raspberry Pi and iPhone app
- Camera frame size: QVGA (320 × 240)
- JPEG capture quality: configured by `CAMERA_JPEG_QUALITY`
- Notification chunk size: configured by `CHUNK_SIZE` (default 500 bytes)
- Acknowledgement window: configured by `ACK_WINDOW` (default 2,000 bytes)
- ACK timeout: configured by `ACK_TIMEOUT_MS` (default 15 seconds)
- Transfer timeout: configured by `TRANSFER_TIMEOUT_MS` (default 30 seconds)

The firmware flushes one buffered camera frame before taking the requested photograph. This avoids returning the previous image when successive captures are requested.

## iPhone app compatibility

The iPhone app selects its acknowledgement window based on the connected camera name:

- Raspberry Pi: 40,000 bytes
- Any XIAO name beginning with `XIAO-`: 2,000 bytes

The app recognises the prefix automatically, so adding another uniquely named XIAO does not require another app code change. The firmware advertises the BLE service UUID in the main advertising packet and puts the configurable device name in the scan response. This keeps the payload within the legacy BLE advertising size limit while allowing the iPhone to discover the service.

## Serial monitor checks

Set the Serial Monitor to **115200 baud**. On startup, the expected messages include:

- `Camera initialised successfully!`
- `Sensor PID: 0x3660`
- `OV3660 ready.`
- `BLE advertising started`
- `Advertising as XIAO-CAMERA-01` (or the name configured in `config.h`)

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
