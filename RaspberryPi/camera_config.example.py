"""Per-device settings for a Raspberry Pi camera.

Copy this file to /home/pi/camera_project/camera_config.py on each Pi,
then change CAMERA_NAME for that particular camera. Keep this file separate
from ble_server.py so software updates do not overwrite device settings.
"""

# Unique Bluetooth name shown in the iPhone app.
# Keep it short and do not start it with "XIAO-" (that prefix is reserved
# for XIAO cameras by the iPhone app).
CAMERA_NAME = "POS-URINE-AN"

# Raspberry Pi camera capture settings.
CAPTURE_WIDTH = 2304
CAPTURE_HEIGHT = 1296
JPEG_QUALITY = 85
CAPTURE_TIMEOUT_MS = 1000

# BLE transfer settings.
# Leave these values unchanged unless the matching iPhone settings are
# changed and tested at the same time.
CHUNK_SIZE = 180
NOTIFY_PAYLOAD_SIZE = 244
TRANSFER_WINDOW_SIZE = 80032
TRANSFER_ACK_TIMEOUT = 15
