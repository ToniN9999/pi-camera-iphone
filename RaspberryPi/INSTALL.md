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

Copy the camera server:

```bash
cp ~/pi-camera-iphone/RaspberryPi/ble_server.py ~/camera_project/
```

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