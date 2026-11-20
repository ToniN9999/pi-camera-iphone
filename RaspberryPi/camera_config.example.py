"""Per-device settings for a Raspberry Pi camera.

INSTALL / UPDATE:
1. Copy this template to /home/pi/camera_project/camera_config.py.
2. Edit that local copy for this physical Pi.
3. After changing this file, restart the service (no Pi reboot needed):
       sudo systemctl restart pi-camera-ble.service
4. Check the startup log:
       sudo journalctl -u pi-camera-ble.service -n 30 --no-pager

Do not overwrite the local camera_config.py when updating ble_server.py.
The common server code is the same on every Raspberry Pi.
"""

# ---------------------------------------------------------------------
# BLUETOOTH DEVICE NAME
# ---------------------------------------------------------------------

# Unique name displayed in the iPhone app.
# Valid: text string. Use simple ASCII letters, numbers and hyphens.
# Recommendation: 1-20 characters, unique for each camera.
# IMPORTANT: Pi names must NOT start with "XIAO-" (reserved for XIAO).
CAMERA_NAME = "POS-URINE-AN"


# ---------------------------------------------------------------------
# RASPBERRY PI CAMERA CAPTURE
# ---------------------------------------------------------------------

# Output image width and height in pixels.
# Valid: positive whole numbers. These are output dimensions, not a
# fixed list of sensor frame-size enums; rpicam-still scales the output.
# The maximum practical size depends on the connected camera sensor.
# To inspect the sensor modes available on this Pi, run:
#       rpicam-still --list-cameras
# Common output examples: 640x480, 1280x720, 1920x1080, 2304x1296.
# Current default is 2304x1296.
CAPTURE_WIDTH = 2304
CAPTURE_HEIGHT = 1296

# JPEG quality.
# Valid: integer 1-100. 1 = smallest / lowest quality;
# 100 = highest quality / usually largest file.
# Current default: 85.
JPEG_QUALITY = 85

# rpicam-still capture timeout, in milliseconds.
# Valid: positive whole number of milliseconds. 1000 = 1 second.
# Increase if the camera needs longer to settle before capture.
CAPTURE_TIMEOUT_MS = 1000


# ---------------------------------------------------------------------
# BLUETOOTH LOW ENERGY TRANSFER
# ---------------------------------------------------------------------

# Legacy read-characteristic chunk size (not used by the current
# iPhone notification-stream transfer path).
# Valid: positive whole number; keep at 180 unless using the legacy read path.
CHUNK_SIZE = 180

# Notification value size in bytes.
# Keep at 244 for the tested Pi 251-byte link-layer packet:
# 4-byte L2CAP header + 3-byte ATT header + 244-byte value = 251 bytes.
# Do not change for normal per-device configuration.
NOTIFY_PAYLOAD_SIZE = 244

# Bytes transferred before an acknowledgement is expected.
# MUST match the Raspberry Pi ACK window in
# PiCamera/CameraBluetoothManager.swift in the iPhone app.
# Current tested value: 80032. Do not change on the Pi alone.
TRANSFER_WINDOW_SIZE = 80032

# Time allowed for the iPhone acknowledgement, in seconds.
# Valid: positive whole number of seconds. Current value: 15.
TRANSFER_ACK_TIMEOUT = 15
