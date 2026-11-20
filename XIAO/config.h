#pragma once

// Give each XIAO camera a unique Bluetooth name.
// Change this file for each physical XIAO; leave the main sketch unchanged.
#define CAMERA_NAME "XIAO-CAMERA-01"

// BLE image-transfer settings
#define CHUNK_SIZE 500
#define ACK_WINDOW 2000
#define ACK_TIMEOUT_MS 15000
#define TRANSFER_TIMEOUT_MS 30000

// Camera capture settings.
// Examples include FRAMESIZE_QVGA (320x240), FRAMESIZE_VGA (640x480)
// and FRAMESIZE_SVGA (800x600), subject to sensor and memory limits.
#define CAMERA_FRAME_SIZE FRAMESIZE_QVGA

// Camera JPEG quality (lower number = higher quality).
#define CAMERA_JPEG_QUALITY 12
