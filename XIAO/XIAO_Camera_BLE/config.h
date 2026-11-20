#pragma once

/*
 * XIAO ESP32S3 SENSE - PER-DEVICE CONFIGURATION
 *
 * Keep this file in the same Arduino sketch folder as
 * XIAO_Camera_BLE.ino.
 *
 * AFTER CHANGING THIS FILE:
 * 1. Open XIAO_Camera_BLE.ino in Arduino IDE.
 * 2. Select board XIAO_ESP32S3 and the correct USB port.
 * 3. Select Tools > PSRAM > OPI PSRAM.
 * 4. Click Upload. The XIAO must be reflashed for changes to take effect.
 *
 * The settings below are compiled into the firmware; editing this file
 * alone does not change the running camera.
 */

// ---------------------------------------------------------------------
// BLUETOOTH DEVICE NAME
// ---------------------------------------------------------------------

// Each physical XIAO needs a unique name.
// Valid: text string. Keep the "XIAO-" prefix so the iPhone app recognises
// this as a XIAO camera. Use simple ASCII letters, numbers and hyphens.
// Recommendation: keep the complete name to 20 characters or fewer so
// it fits comfortably in the BLE scan response.
#define CAMERA_NAME "XIAO-CAMERA-01"


// ---------------------------------------------------------------------
// CAMERA FRAME SIZE AND JPEG QUALITY
// ---------------------------------------------------------------------

// Select ONE frame-size constant from the list below.
// The OV3660 sensor used by this project supports up to QXGA (2048x1536).
// Smaller sizes reduce image detail and usually reduce JPEG file size.
//
// Valid frame-size constants at or below the OV3660's QXGA limit:
//   FRAMESIZE_96X96     = 96x96
//   FRAMESIZE_QQVGA     = 160x120
//   FRAMESIZE_128X128   = 128x128
//   FRAMESIZE_QCIF      = 176x144
//   FRAMESIZE_HQVGA     = 240x176
//   FRAMESIZE_240X240   = 240x240
//   FRAMESIZE_QVGA      = 320x240
//   FRAMESIZE_320X320   = 320x320
//   FRAMESIZE_CIF       = 400x296
//   FRAMESIZE_HVGA      = 480x320
//   FRAMESIZE_VGA       = 640x480
//   FRAMESIZE_SVGA      = 800x600
//   FRAMESIZE_XGA       = 1024x768
//   FRAMESIZE_HD        = 1280x720
//   FRAMESIZE_SXGA      = 1280x1024
//   FRAMESIZE_UXGA      = 1600x1200
//   FRAMESIZE_FHD       = 1920x1080
//   FRAMESIZE_P_HD      = 720x1280 (portrait)
//   FRAMESIZE_P_3MP     = 864x1536 (portrait)
//   FRAMESIZE_QXGA      = 2048x1536 (maximum OV3660 frame size)
//
// Do not select sizes above QXGA for this OV3660 camera.
#define CAMERA_FRAME_SIZE FRAMESIZE_UXGA

// JPEG quality for the ESP32 camera driver.
// Valid: integer 0-63. LOWER numbers mean HIGHER image quality and
// usually larger files. Higher numbers mean more compression.
// Current setting 12 is the existing tested value.
#define CAMERA_JPEG_QUALITY 12


// ---------------------------------------------------------------------
// BLUETOOTH LOW ENERGY TRANSFER
// ---------------------------------------------------------------------

// Notification data chunk in bytes.
// Valid with the configured BLE MTU of 517: 1-514 bytes.
// The current 500-byte setting is retained. Changing this can affect
// throughput and ACK alignment; test before changing.
#define CHUNK_SIZE 500

// Bytes received before the iPhone sends an acknowledgement.
// MUST remain 2000 to match the XIAO ACK window in
// PiCamera/CameraBluetoothManager.swift. Do not change on the XIAO alone.
// For clean chunk alignment, use a multiple of CHUNK_SIZE (500 currently).
#define ACK_WINDOW 2000

// Time allowed for an iPhone acknowledgement, in milliseconds.
// Valid: positive whole number of milliseconds. Current value: 15000.
#define ACK_TIMEOUT_MS 15000

// No-progress timeout, in milliseconds.
// Valid: positive whole number of milliseconds. The firmware aborts if
// no successful BLE notification is sent for this long. Current: 30000.
// Keep this comfortably longer than ACK_TIMEOUT_MS.
#define TRANSFER_TIMEOUT_MS 30000
