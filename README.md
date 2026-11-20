# Pi Camera iPhone

Native SwiftUI/Core Bluetooth iPhone app and Raspberry Pi BLE camera
server.

The system supports Raspberry Pi 5 cameras and a Seeed Studio XIAO ESP32S3 Sense camera. Each Raspberry Pi advertises the unique name set in its local `camera_config.py`; each XIAO advertises the unique name set in `XIAO/XIAO_Camera_BLE/config.h`. The iPhone app discovers compatible cameras and lets the user choose which one to connect to.

## Repository structure

-   `PiCamera/` --- native iOS application source.
-   `RaspberryPi/ble_server.py` --- common Raspberry Pi BLE camera server.
-   `RaspberryPi/camera_config.example.py` --- template for each Pi's local camera name, capture and transfer settings.
-   `RaspberryPi/pi-camera-ble.service` --- systemd service for starting
    the camera server.
-   `RaspberryPi/pi-camera-watchdog.sh` --- checks that Bluetooth
    advertising is active and restarts the BLE service if advertising
    is missing.
-   `RaspberryPi/pi-camera-watchdog.service` and
    `RaspberryPi/pi-camera-watchdog.timer` --- systemd units that run
    the watchdog once per minute.
-   `RaspberryPi/requirements.txt` --- Python dependencies for the
    Raspberry Pi server.
-   `RaspberryPi/INSTALL.md` --- Raspberry Pi installation instructions.
-   `XIAO/XIAO_Camera_BLE/` --- Arduino sketch folder containing `XIAO_Camera_BLE.ino` and `config.h`.
-   `XIAO/XIAO_Camera_BLE/config.h` --- per-device XIAO name and camera/transfer settings; edit this instead of the main sketch.
-   `XIAO/INSTALL.md` --- XIAO Arduino IDE setup and firmware configuration.
-   `.github/workflows/` --- GitHub Actions workflow for building the
    iOS app.

## Multiple-camera operation

Each Raspberry Pi gets a local `camera_config.py`, copied from
`RaspberryPi/camera_config.example.py`. Set `CAMERA_NAME` in that file to
a unique, short name for that physical Pi (for example, `POS-URINE-AN`
or `WORKSHOP-CAMERA`). Do not edit `ble_server.py` for individual Pi
installations. Keep the Pi name distinct from the reserved `XIAO-`
prefix because the iPhone app uses that prefix to identify XIAO devices.

Each XIAO gets a unique name in `XIAO/XIAO_Camera_BLE/config.h` (for example,
`XIAO-CAMERA-01` and `XIAO-CAMERA-02`). Keep the `XIAO-` prefix: the
iPhone app uses it to select the XIAO acknowledgement window. The app scans
for the camera service UUID and displays discovered compatible devices by
their Bluetooth names. Select the required camera in the app before connecting.

### Applying configuration changes

- **Raspberry Pi:** edit the local `/home/pi/camera_project/camera_config.py`,
  then run `sudo systemctl restart pi-camera-ble.service`. A full Pi reboot
  is not normally needed.
- **XIAO:** edit `XIAO/XIAO_Camera_BLE/config.h`, then open
  `XIAO/XIAO_Camera_BLE/XIAO_Camera_BLE.ino` in Arduino IDE and upload
  the firmware again. The XIAO must be reflashed; restarting the Pi does
  not apply XIAO settings.

## Bluetooth protocol

The Raspberry Pi server and iPhone app use the following custom BLE
service and characteristics:

  Purpose             UUID
  ------------------- ----------------------------------------
  Service             `7f510001-1b15-4f0e-9b7b-8c8f8f8f0001`
  Command             `7f510002-1b15-4f0e-9b7b-8c8f8f8f0001`
  Image information   `7f510003-1b15-4f0e-9b7b-8c8f8f8f0001`
  Image request       `7f510004-1b15-4f0e-9b7b-8c8f8f8f0001`
  Image data          `7f510005-1b15-4f0e-9b7b-8c8f8f8f0001`
  Image stream        `7f510006-1b15-4f0e-9b7b-8c8f8f8f0001`
  Transfer control    `7f510007-1b15-4f0e-9b7b-8c8f8f8f0001`

The app sends a capture command, waits for the capture status, obtains the JPEG size, receives the image stream and acknowledges transfer windows. It displays the received image and can save it to Photos. The acknowledgement window is camera-specific: 80,032 bytes for Raspberry Pi and 2,000 bytes for any camera whose name begins with `XIAO-`. The Raspberry Pi sends image notifications with a 244-byte value payload. With the tested 251-byte Bluetooth LE link-layer data length, each notification fits in one link-layer PDU (4-byte L2CAP header + 3-byte ATT notification header + 244-byte value). The Pi transfer window is 80,032 bytes (328 × 244-byte notifications), keeping acknowledgement boundaries aligned. XIAO firmware retains its own configurable notification chunk size and 2,000-byte acknowledgement window.

## Raspberry Pi image capture settings

The Pi camera server's default capture is **2304 × 1296** pixels at JPEG quality **85**. Actual JPEG size depends on scene detail and lighting. Each Pi's `camera_config.py` controls its camera name, capture dimensions, JPEG quality and transfer constants; the common server code stays the same across devices.

## Raspberry Pi BLE transfer settings

The current experimental Raspberry Pi server uses these transfer settings:

- Notification value payload: **244 bytes**.
- Transfer/acknowledgement window: **80,032 bytes** (328 notifications).
- ATT MTU observed in the tested connection: **517**.
- Bluetooth LE link-layer data length observed in the tested connection: **251 bytes**.

The 244-byte notification value is sized to fit one 251-byte link-layer PDU after the 4-byte L2CAP and 3-byte ATT headers. In tests with the Raspberry Pi 5 and iPhone, this improved image-transfer throughput compared with the previous 500-byte notification value. Actual throughput varies with the BLE connection and radio conditions; these figures are configuration details, not a guaranteed rate.

Keep the Pi's `NOTIFY_PAYLOAD_SIZE` and `TRANSFER_WINDOW_SIZE` in its local `camera_config.py` consistent with the iPhone's Raspberry Pi acknowledgement window in `PiCamera/CameraBluetoothManager.swift`. The iPhone must acknowledge the same 80,032-byte boundaries used by the Pi. This doubled window is experimental; the previous 40,016-byte setting remains available in the preceding Git commit as the known-good baseline.

## Raspberry Pi installation

Follow [`RaspberryPi/INSTALL.md`](RaspberryPi/INSTALL.md) for the
complete installation procedure, including dependencies, service setup
and watchdog configuration.

Install the common server on each Raspberry Pi. Copy
`RaspberryPi/camera_config.example.py` to
`/home/pi/camera_project/camera_config.py` on each Pi, then change only
`CAMERA_NAME` to give that unit a unique Bluetooth name. Keep each Pi's
local config file when deploying later server updates.

The Raspberry Pi camera server runs locally. The iPhone communicates
with it over Bluetooth Low Energy; Wi-Fi or an internet connection is
not required for camera capture and image transfer.

## XIAO camera setup

Follow [`XIAO/INSTALL.md`](XIAO/INSTALL.md) to configure the Seeed Studio
XIAO ESP32S3 Sense with OV3660 camera using Arduino IDE. Use **ESP32
Arduino core 4.0.0** and the core's built-in BLE API; NimBLE-Arduino is
not required. The required PSRAM setting is **OPI PSRAM**.

The currently tested XIAO configuration is:

- Camera frame size: **UXGA (1600 × 1200)**.
- JPEG quality: **12**.
- BLE notification chunk: **500 bytes**.
- Acknowledgement window: **2,000 bytes**.
- ACK timeout: **15 seconds**.
- Transfer timeout: **30 seconds**.

Five consecutive UXGA captures and BLE transfers completed successfully
during testing. The iPhone app was also tested switching between the
Raspberry Pi and XIAO cameras and successfully capturing images from both.
JPEG size and transfer time vary with the scene. Image compression and
transfer tuning are intentionally left for later optimisation.

## Building and releasing the iPhone app

GitHub Actions builds an unsigned iOS IPA on a macOS runner whenever
changes are pushed to `main`. The IPA is retained as a downloadable
Actions artifact for seven days.

To create a permanent versioned download, push a version tag in the
form `vX.Y.Z` (for example, `v1.0.0`). The workflow builds the app
and publishes `PiCamera-unsigned.ipa` as an asset attached to the
matching GitHub Release. Releases retain the IPA alongside the source
code for that version.

The IPA is **unsigned**. Signing and provisioning are required before
installing it on an iPhone; use a suitable sideloading tool and Apple
signing credentials. With a free Apple Developer Personal Team,
provisioning typically needs to be renewed every seven days.

## Development notes

-   Keep the BLE UUIDs and transfer protocol settings in the iOS app and
    Raspberry Pi server in sync.
-   Test server changes on a Raspberry Pi before deploying them to other
    cameras.
-   The watchdog checks Bluetooth advertising once per minute and
    restarts the camera BLE service if no advertising instance is
    active.
