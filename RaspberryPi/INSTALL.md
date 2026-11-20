# Raspberry Pi 5 – Camera Installation Guide

## 1. Hardware requirements

- Raspberry Pi 5
- Raspberry Pi Camera Module 3
- Raspberry Pi OS (64-bit)
- Bluetooth enabled

These instructions assume the Raspberry Pi username is `pi`.

## 2. Update Raspberry Pi OS

Connect to the Pi using SSH and run:

```bash
sudo apt update
sudo apt full-upgrade -y
```

Install the required system packages:

```bash
sudo apt install -y bluez python3-venv python3-pip rpicam-apps git
```

Enable Bluetooth:

```bash
sudo systemctl enable --now bluetooth
```

Check that the camera is detected:

```bash
rpicam-still --list-cameras
```

## 3. Download the project

Clone the GitHub repository:

```bash
git clone https://github.com/ToniN9999/pi-camera-iphone.git ~/pi-camera-iphone
```

The repository is private, so GitHub authentication will be required.

## 4. Create the camera project directory

```bash
mkdir -p ~/camera_project/captures
```

Copy the common camera server and the example configuration:

```bash
cp ~/pi-camera-iphone/RaspberryPi/ble_server.py ~/camera_project/
cp ~/pi-camera-iphone/RaspberryPi/camera_config.example.py ~/camera_project/camera_config.py
```

Edit the local configuration for this physical Pi:

```bash
nano ~/camera_project/camera_config.py
```

Set `CAMERA_NAME` to a unique, short name for this camera. For the
existing Pi, the example value is `POS-URINE-AN`. Give every additional
Pi a different name. Do not use the `XIAO-` prefix; the iPhone app
reserves it for XIAO cameras.

The configuration file lives in `/home/pi/camera_project`, separate
from the common server code. Do not overwrite this local file when
deploying later server updates.

## 5. Create the Python virtual environment

```bash
cd ~/camera_project

python3 -m venv .venv

source .venv/bin/activate

pip install --upgrade pip

pip install -r ~/pi-camera-iphone/RaspberryPi/requirements.txt
```

The Python dependencies are:

- `bluez-peripheral`
- `dbus-next`

## 6. Install the camera BLE service

Copy the systemd service file:

```bash
sudo cp ~/pi-camera-iphone/RaspberryPi/pi-camera-ble.service /etc/systemd/system/
```

Reload systemd:

```bash
sudo systemctl daemon-reload
```

Enable the camera service:

```bash
sudo systemctl enable --now pi-camera-ble.service
```

Check its status:

```bash
sudo systemctl status pi-camera-ble.service --no-pager
```

## 7. Install the Bluetooth advertising watchdog

Copy the watchdog script:

```bash
sudo cp ~/pi-camera-iphone/RaspberryPi/pi-camera-watchdog.sh /usr/local/bin/
```

Set executable permissions:

```bash
sudo chmod +x /usr/local/bin/pi-camera-watchdog.sh
```

Copy the watchdog systemd files:

```bash
sudo cp ~/pi-camera-iphone/RaspberryPi/pi-camera-watchdog.service /etc/systemd/system/

sudo cp ~/pi-camera-iphone/RaspberryPi/pi-camera-watchdog.timer /etc/systemd/system/
```

Reload systemd:

```bash
sudo systemctl daemon-reload
```

Enable the watchdog timer:

```bash
sudo systemctl enable --now pi-camera-watchdog.timer
```

Check the timer:

```bash
systemctl status pi-camera-watchdog.timer --no-pager
```

## 8. Verify Bluetooth advertising

Run:

```bash
bluetoothctl show
```

Look for:

```text
Powered: yes
Pairable: no
ActiveInstances: 0x01 (1)
```

The active advertising instance confirms that Bluetooth advertising is running.

## 9. Test the camera

Check the camera service:

```bash
sudo systemctl status pi-camera-ble.service --no-pager
```

Check the logs:

```bash
sudo journalctl -u pi-camera-ble.service -n 25 --no-pager
```

Open the Pi Camera iPhone app, scan for cameras and select the Raspberry Pi by its advertised device name.

Capture an image and verify that it transfers successfully and can be saved to Photos.

## 10. Useful maintenance commands

Restart the camera service:

```bash
sudo systemctl restart pi-camera-ble.service
```

View live camera logs:

```bash
sudo journalctl -u pi-camera-ble.service -f
```

Check watchdog status:

```bash
systemctl status pi-camera-watchdog.timer --no-pager
```

Run a manual watchdog check:

```bash
sudo systemctl start pi-camera-watchdog.service
```

## Raspberry Pi configuration

Each Pi has its own `/home/pi/camera_project/camera_config.py`.
Configure the following values there:

- `CAMERA_NAME`: unique Bluetooth name displayed by the iPhone app.
- `CAPTURE_WIDTH` and `CAPTURE_HEIGHT`: image dimensions (default **2304 × 1296**).
- `JPEG_QUALITY`: JPEG quality (default **85**; higher numbers mean less compression).
- `CAPTURE_TIMEOUT_MS`: camera capture timeout passed to `rpicam-still`.
- `CHUNK_SIZE`, `NOTIFY_PAYLOAD_SIZE`, `TRANSFER_WINDOW_SIZE` and `TRANSFER_ACK_TIMEOUT`: BLE transfer constants.

The repository's `camera_config.example.py` is a template. Copy it to
`camera_config.py` once on each Pi, then keep that local file when
updating `ble_server.py`. The common server code is the same on every
Pi; per-device differences belong in `camera_config.py`.

Keep Pi camera names short and do not begin them with `XIAO-`. That
prefix is used by the iPhone app to select the XIAO transfer settings.
The Pi BLE window and notification payload must remain aligned with the
matching values in the iPhone app; change them only as a coordinated,
tested change.

### After changing camera_config.py

The Pi reads this file when the BLE server starts. After saving changes,
restart the service; a full Pi reboot is not normally required:

```bash
sudo systemctl restart pi-camera-ble.service
sudo journalctl -u pi-camera-ble.service -n 30 --no-pager
```

Check the startup log to confirm the camera name, image dimensions,
JPEG quality and BLE settings are the values you intended. If the service
fails to start, check the error shown by the journal before taking a photo.

## Raspberry Pi BLE transfer settings

The current experimental `RaspberryPi/ble_server.py` uses:

- Notification payload: **244 bytes**.
- Transfer acknowledgement window: **80,032 bytes** (328 × 244-byte notifications).
- ATT MTU observed in the tested Pi/iPhone connection: **517**.
- Link-layer data length observed in that connection: **251 bytes**.

The 244-byte notification value plus the 3-byte ATT notification header and 4-byte L2CAP header totals 251 bytes, matching the negotiated link-layer data length in the tested setup. The iPhone app uses the matching 80,032-byte acknowledgement window for Raspberry Pi cameras. Do not change one side without changing the other. The 80,032-byte window is an experiment; the previous 40,016-byte setting is preserved in the preceding Git commit as the known-good baseline. XIAO cameras use separate firmware settings and a 2,000-byte acknowledgement window.

To deploy an updated common server from the cloned repository, replace
only `ble_server.py`. Leave the Pi's local `camera_config.py` untouched.
Then compile-check and restart the service:

```bash
cp ~/pi-camera-iphone/RaspberryPi/ble_server.py ~/camera_project/ble_server.py
python3 -m py_compile ~/camera_project/ble_server.py
sudo systemctl restart pi-camera-ble.service
sudo journalctl -u pi-camera-ble.service -n 30 --no-pager
```

The startup log should report the configured camera name and capture
settings, followed by `Notification payload: 244` and
`Notification window: 80032`.

## Useful BLE transfer diagnostics

View recent transfer logs:

```bash
sudo journalctl -u pi-camera-ble.service -n 100 --no-pager
```

Capture Bluetooth traffic while reproducing a transfer:

```bash
sudo btmon -w /tmp/pi-camera-bluetooth.snoop
```

Stop the capture with Ctrl+C, then decode it:

```bash
sudo btmon -r /tmp/pi-camera-bluetooth.snoop > /tmp/bt-analysis.txt
```

Count notifications and HCI ACL packets in the decoded capture:

```bash
grep -c "ATT: Handle Value Notification" /tmp/bt-analysis.txt
grep -c "ACL Data TX" /tmp/bt-analysis.txt
grep -c "ACL Data RX" /tmp/bt-analysis.txt
```

## Important configuration notes

The camera service currently expects:

- Project directory: `/home/pi/camera_project`
- Python environment: `/home/pi/camera_project/.venv`
- Python server: `/home/pi/camera_project/ble_server.py`
- Captures directory: `/home/pi/camera_project/captures`

The systemd service and watchdog files are configured for this directory structure.

The iPhone application communicates with the camera using the custom BLE service UUID documented in the main project README.

Keep the BLE UUIDs and transfer protocol compatible when modifying the server.

**Note:** These instructions document the existing Raspberry Pi 5 configuration. Verify the camera service and image transfer on a fresh installation before relying on it in production.