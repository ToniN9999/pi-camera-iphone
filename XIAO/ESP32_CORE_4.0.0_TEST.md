# XIAO ESP32 Arduino core 4.0.0 compatibility test

This document records the completed migration and validation of the XIAO
ESP32S3 Sense camera firmware from NimBLE-Arduino to the BLE API built into
ESP32 Arduino core 4.0.0.

## Why the BLE implementation changed

The original NimBLE-Arduino 2.5.2 build failed against ESP32 Arduino core
4.0.0 in `ble_hs_periodic_sync.c` because of an incompatible structure
member. Rather than changing the working Raspberry Pi or iPhone protocol,
the XIAO sketch was migrated to the built-in ESP32 Core 4 BLE API:

- Include: `#include <BLE.h>`
- No separate NimBLE-Arduino library is required.
- The existing service and characteristic UUIDs are retained.
- The existing command, image information, notification stream and ACK
  protocol are retained.
- Command characteristic values are written with explicit byte lengths.
  This is important because the iPhone compares the status strings exactly;
  including a trailing null byte caused the capture-complete status to be
  missed.

## Test environment

- Board: Seeed Studio XIAO ESP32S3 Sense
- ESP32 Arduino core: **4.0.0**
- BLE implementation: built-in ESP32 Core 4 BLE API
- Camera: OV3660 (sensor PID `0x3660`)
- PSRAM: OPI PSRAM
- Camera frame size in the committed configuration: UXGA (1600 × 1200)
- JPEG quality: 12
- Notification chunk: 500 bytes
- Acknowledgement window: 2,000 bytes

## Validation results

The firmware compiled and uploaded successfully. The camera initialised,
the XIAO advertised its configured Bluetooth name, and the iPhone app
connected and received photographs.

Five consecutive UXGA captures completed successfully. The recorded JPEG
sizes were:

| Capture | JPEG size | Transfer time |
|---:|---:|---:|
| 1 | 148,528 bytes | 6.85 s |
| 2 | 104,986 bytes | 4.85 s |
| 3 | 138,900 bytes | 6.41 s |
| 4 | 115,830 bytes | 5.16 s |
| 5 | 111,687 bytes | 5.23 s |

All five transfers completed with acknowledgement progress and no reported
ACK timeout. Average image size was approximately 124 KB and average
transfer time was approximately 5.70 seconds. JPEG size and transfer time
vary with the scene.

The iPhone app was also tested switching between the Raspberry Pi and XIAO
cameras twice, successfully capturing images from both cameras.

## Current known-good settings

See `XIAO/XIAO_Camera_BLE/config.h` for the committed settings. The current
UXGA and JPEG quality 12 combination is retained as the working baseline.
Further image quality and transfer-speed optimisation is deferred.

## Setup

Follow [`INSTALL.md`](INSTALL.md) for the current Arduino IDE installation
and firmware upload instructions. Those instructions target ESP32 Arduino
core 4.0.0 and the built-in BLE API; do not install NimBLE-Arduino for this
sketch.
