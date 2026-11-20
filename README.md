# Pi Camera iPhone

Native SwiftUI/Core Bluetooth iPhone app and Raspberry Pi BLE camera
server.

The system supports Raspberry Pi 5 cameras and a Seeed Studio XIAO ESP32S3 Sense camera. Each Raspberry Pi advertises its hostname over Bluetooth. Each XIAO advertises a configurable unique name (for example, `XIAO-CAMERA-01`). The iPhone app discovers compatible cameras and lets the user choose which one to connect to.

## Repository structure

-   `PiCamera/` --- native iOS application source.
-   `RaspberryPi/ble_server.py` --- Raspberry Pi BLE camera server.
-   `RaspberryPi/pi-camera-ble.service` --- systemd service for starting
    the camera server.
-   `RaspberryPi/pi-camera-watchdog.sh` --- checks that Bluetooth
    advertising is active and restarts the BLE service if advertising is
    missing.
-   `RaspberryPi/pi-camera-watchdog.service` and
    `RaspberryPi/pi-camera-watchdog.timer` --- systemd units that run
    the watchdog once per minute.
-   `RaspberryPi/requirements.txt` --- Python dependencies for the
    Raspberry Pi server.
-   `RaspberryPi/INSTALL.md` --- Raspberry Pi installation instructions.
-   `XIAO/XIAO_Camera_BLE.ino` --- tested XIAO ESP32S3 Sense BLE camera firmware.
-   `XIAO/config.h` --- per-device XIAO name and camera/transfer settings; edit this instead of the main sketch.
-   `XIAO/INSTALL.md` --- XIAO Arduino IDE setup and firmware configuration.
-   `.github/workflows/` --- GitHub Actions workflow for building the
    iOS app.

## Multiple-camera operation

Each Raspberry Pi uses its own Linux hostname as its Bluetooth device
name. The server obtains this automatically using:

``` python
DEVICE_NAME = os.uname().nodename
```

For example, a Pi with hostname `POS-URINE-AN` advertises as
`POS-URINE-AN`; another Pi with hostname `WORKSHOP-CAMERA` advertises as
`WORKSHOP-CAMERA`.

The iPhone app scans for the camera service UUID and displays discovered compatible devices by their Bluetooth names. Select the required camera in the app before connecting. XIAO devices should use unique names beginning with `XIAO-`, configured in `XIAO/config.h` (for example, `XIAO-CAMERA-01` and `XIAO-CAMERA-02`).

Give each Pi a unique hostname so that cameras can be distinguished in
the selection list.

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

The app sends a capture command, waits for the capture status, obtains the JPEG size, receives the image stream and acknowledges transfer windows. It displays the received image and can save it to Photos. The acknowledgement window is camera-specific: 40,000 bytes for Raspberry Pi and 2,000 bytes for any camera whose name begins with `XIAO-`.

## Raspberry Pi installation

Follow [`RaspberryPi/INSTALL.md`](RaspberryPi/INSTALL.md) for the
complete installation procedure, including dependencies, service setup
and watchdog configuration.

Install the server on each Raspberry Pi that should act as a camera. Set
a unique hostname on each Pi before installation. The server derives its
Bluetooth name from that hostname; no per-device name edit in
`ble_server.py` should be necessary.

The Raspberry Pi camera server runs locally. The iPhone communicates
with it over Bluetooth Low Energy; Wi-Fi or an internet connection is
not required for camera capture and image transfer.

## XIAO camera setup

Follow [`XIAO/INSTALL.md`](XIAO/INSTALL.md) to configure the Seeed Studio XIAO ESP32S3 Sense with OV3660 camera using Arduino IDE. The required PSRAM setting is **OPI PSRAM**.

## Building the iPhone app

GitHub Actions builds an unsigned iOS IPA on a macOS runner. Download
the IPA artifact from a successful Actions run, then sign and install it
on the iPhone using a suitable sideloading tool and Apple signing
credentials.

With a free Apple Developer Personal Team, provisioning typically needs
to be renewed every seven days.

## Development notes

-   Keep the BLE UUIDs and transfer protocol settings in the iOS app and
    Raspberry Pi server in sync.
-   Test server changes on a Raspberry Pi before deploying them to other
    cameras.
-   The watchdog checks Bluetooth advertising once per minute and
    restarts the camera BLE service if no advertising instance is
    active.
