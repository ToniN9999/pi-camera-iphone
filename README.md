# Pi Camera – iPhone App

A native SwiftUI/Core Bluetooth iPhone application for controlling a Raspberry Pi BLE camera server.

The project contains both the iPhone application and the Raspberry Pi camera server.

## Project structure

```text
pi-camera-iphone/
│
├── PiCamera/
│   ├── PiCameraApp.swift
│   └── CameraBluetoothManager.swift
│
├── RaspberryPi/
│   ├── ble_server.py
│   ├── pi-camera-ble.service
│   ├── pi-camera-watchdog.sh
│   ├── pi-camera-watchdog.service
│   └── pi-camera-watchdog.timer
│
└── README.md
```

## Hardware

- Raspberry Pi 5
- Raspberry Pi Camera Module 3
- iPhone 17 or compatible iPhone with Bluetooth LE

## iPhone application

Native SwiftUI/Core Bluetooth client.

### Current implementation

- Scans for devices advertising the camera BLE service UUID.
- Displays available cameras using their advertised device names.
- Allows the user to select which camera to connect to.
- Sends `CAPTURE` to characteristic `0002`.
- Polls capture status until `CAPTURED`.
- Reads the JPEG byte count from characteristic `0003`.
- Enables notifications on characteristic `0006`.
- Sends `START` and acknowledges image transfers using characteristic `0007`.
- Uses a 40,000-byte acknowledgement window.
- Displays the received JPEG and allows saving it to Photos.

The application uses Bluetooth LE directly. It does not require Wi-Fi, a web page, Cloudflare or an internet connection at runtime.

## Raspberry Pi server

The Raspberry Pi runs `ble_server.py`, which controls the camera and provides the custom BLE GATT service.

### Camera service

`pi-camera-ble.service`

- Starts the Python BLE camera server.
- Runs automatically at boot.
- Restarts automatically if the server fails.

### Bluetooth advertising watchdog

The watchdog monitors Bluetooth advertising every minute.

Files:

- `pi-camera-watchdog.sh`
- `pi-camera-watchdog.service`
- `pi-camera-watchdog.timer`

If Bluetooth advertising stops, the watchdog restarts the camera BLE service.

### Useful commands

Check camera service:

```bash
sudo systemctl status pi-camera-ble.service --no-pager
```

Restart camera service:

```bash
sudo systemctl restart pi-camera-ble.service
```

View camera service logs:

```bash
sudo journalctl -u pi-camera-ble.service -n 25 --no-pager
```

Check watchdog timer:

```bash
systemctl status pi-camera-watchdog.timer --no-pager
```

View watchdog logs:

```bash
sudo journalctl -u pi-camera-watchdog.service -n 25 --no-pager
```

## Bluetooth protocol

The iPhone communicates with the Raspberry Pi using a custom BLE GATT service.

### UUIDs

| Characteristic | UUID |
|---|---|
| Service | `7f510001-1b15-4f0e-9b7b-8c8f8f8f0001` |
| Command | `7f510002-1b15-4f0e-9b7b-8c8f8f8f0001` |
| Image information | `7f510003-1b15-4f0e-9b7b-8c8f8f8f0001` |
| Image request | `7f510004-1b15-4f0e-9b7b-8c8f8f8f0001` |
| Image data | `7f510005-1b15-4f0e-9b7b-8c8f8f8f0001` |
| Image stream | `7f510006-1b15-4f0e-9b7b-8c8f8f8f0001` |
| Transfer control | `7f510007-1b15-4f0e-9b7b-8c8f8f8f0001` |

### Current transfer configuration

- Chunk size: 180 bytes
- Notification payload: 500 bytes
- Acknowledgement window: 40,000 bytes
- ACK timeout: 15 seconds

## Building the iPhone application

GitHub Actions builds an unsigned IPA using a macOS runner.

The IPA must then be signed and installed on the iPhone using Sideloadly and an Apple ID.

With a free Apple Personal Team provisioning profile, signing typically needs renewing every 7 days.

## Important

The Raspberry Pi server and iPhone application are maintained in the same repository.

The existing Raspberry Pi setup is a working reference implementation. Changes to the iPhone application should preserve compatibility with its BLE protocol.

## Future development

A second camera platform is planned using the Seeed Studio XIAO ESP32S3 Sense.

The intention is to retain compatibility with the existing iPhone application and BLE communication protocol wherever possible.