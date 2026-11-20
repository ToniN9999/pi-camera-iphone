# XIAO test plan: ESP32 Arduino core 4.0.0

This is an isolated compatibility test on the `esp32-core-4.0.0` branch.
The working `main` branch and the known-good ESP32 Arduino core 3.3.13
configuration remain unchanged.

## Why the first test keeps the current BLE library

The sketch uses the separate h2zero `NimBLE-Arduino` library through
`#include <NimBLEDevice.h>`; it does not use Espressif's former
`BLEDevice.h` API. Core 4.0.0 rewrites Espressif's built-in BLE API as
`BLE.h`, but its documentation still describes NimBLE-Arduino as an
option for users who want the more customisable NimBLE interface.

Therefore, first test the existing sketch with the external library before
rewriting the GATT server. The sketch prints both the selected Arduino core
version and NimBLE-Arduino version at startup to make the test unambiguous.

## Test environment

- Board: Seeed XIAO ESP32S3
- ESP32 Arduino core: **4.0.0**
- BLE library: **NimBLE-Arduino 2.5.2**
- Camera: OV3660
- PSRAM: OPI PSRAM
- Sketch: `XIAO/XIAO_Camera_BLE/XIAO_Camera_BLE.ino`
- Configuration: `XIAO/XIAO_Camera_BLE/config.h`

## Windows / Arduino IDE procedure

1. In PowerShell, change to the local repository:
   `cd "$HOME\\Documents\\PiCameraIPhone"`
2. Fetch and check out the test branch:
   `git fetch origin`
   `git checkout esp32-core-4.0.0`
   `git pull --ff-only origin esp32-core-4.0.0`
3. In Arduino IDE Boards Manager, install/select **esp32 by Espressif Systems 4.0.0**. Keep 3.3.13 installed so it remains available for rollback.
4. In Library Manager, install/update **NimBLE-Arduino 2.5.2** by h2zero.
5. Open `XIAO/XIAO_Camera_BLE/XIAO_Camera_BLE.ino`.
6. Select board `XIAO_ESP32S3`, the correct port, and **Tools → PSRAM → OPI PSRAM**.
7. Click Verify (compile) first. Do not upload until compilation succeeds.

## If compilation succeeds

1. Click Upload to flash the test firmware.
2. Open Serial Monitor at 115200 baud.
3. Confirm the first lines identify:
   - ESP32 Arduino core: `4.0.0`
   - NimBLE-Arduino: `2.5.2`
4. Confirm the OV3660 initialises and BLE advertises as `XIAO-CAMERA-01`.
5. Connect using the iPhone app and take a photo.
6. Check the iPhone diagnostics and XIAO Serial Monitor for complete transfer and both ACKs.

A successful compile alone is not a successful compatibility test; camera
initialisation, advertising, iPhone connection and full image transfer must
also pass.

## If compilation fails

Do not upload. Copy the complete first compiler error and the surrounding
output. Keep the test branch and the known-good 3.3.13 setup unchanged while
the incompatibility is investigated.

## Roll back

In Arduino IDE, select ESP32 Arduino core **3.3.13** again and reopen the
working sketch from the `main` branch. No change to the Pi server or iPhone
app is required for this core-version test.
