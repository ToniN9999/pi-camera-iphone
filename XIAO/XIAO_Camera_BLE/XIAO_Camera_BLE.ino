#include <BLE.h>
#include "esp_camera.h"
#include "config.h"

// BLE UUIDs matching the Raspberry Pi and iPhone app
#define SERVICE_UUID          "7f510001-1b15-4f0e-9b7b-8c8f8f8f0001"
#define COMMAND_UUID          "7f510002-1b15-4f0e-9b7b-8c8f8f8f0001"
#define IMAGE_INFO_UUID       "7f510003-1b15-4f0e-9b7b-8c8f8f8f0001"
#define IMAGE_REQUEST_UUID    "7f510004-1b15-4f0e-9b7b-8c8f8f8f0001"
#define IMAGE_DATA_UUID       "7f510005-1b15-4f0e-9b7b-8c8f8f8f0001"
#define IMAGE_STREAM_UUID     "7f510006-1b15-4f0e-9b7b-8c8f8f8f0001"
#define TRANSFER_CONTROL_UUID "7f510007-1b15-4f0e-9b7b-8c8f8f8f0001"

// XIAO ESP32S3 Sense - OV3660 camera pins
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39
#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

// Core 4.0 BLE handles are lightweight shared handles, not pointers.
BLEServer bleServer;
BLECharacteristic commandCharacteristic;
BLECharacteristic imageInfoCharacteristic;
BLECharacteristic imageStreamCharacteristic;
BLECharacteristic transferControlCharacteristic;

camera_fb_t* capturedImage = nullptr;

volatile bool transferActive = false;
volatile bool waitingForAck = false;
volatile bool captureRequested = false;
volatile bool disconnectCleanupRequested = false;
volatile bool transferStartRequested = false;
volatile bool ackReceived = false;
volatile uint32_t receivedAckOffset = 0;

size_t transferOffset = 0;
size_t lastAckOffset = 0;

unsigned long lastAckTime = 0;
unsigned long lastTransferTime = 0;

bool initialiseCamera() {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = CAMERA_FRAME_SIZE;
  config.jpeg_quality = CAMERA_JPEG_QUALITY;
  config.fb_count = 1;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera initialisation FAILED: 0x%x\n", err);
    return false;
  }

  Serial.println("Camera initialised successfully!");
  sensor_t* sensor = esp_camera_sensor_get();
  if (sensor != nullptr) {
    Serial.printf("Sensor PID: 0x%04X\n", sensor->id.PID);
  }
  return true;
}

void releaseCapturedImage() {
  if (capturedImage != nullptr) {
    esp_camera_fb_return(capturedImage);
    capturedImage = nullptr;
  }
  transferActive = false;
  waitingForAck = false;
  transferOffset = 0;
  lastAckOffset = 0;
}

bool capturePhoto() {
  releaseCapturedImage();
  Serial.println("Capturing photograph...");
  Serial.println("Flushing old camera frame...");
  camera_fb_t* oldFrame = esp_camera_fb_get();

  if (oldFrame != nullptr) {
    esp_camera_fb_return(oldFrame);
    Serial.println("Old frame discarded.");
  } else {
    Serial.println("No old frame available.");
  }

  delay(150);
  Serial.println("Capturing fresh frame...");
  capturedImage = esp_camera_fb_get();

  if (capturedImage == nullptr) {
    Serial.println("ERROR: Photograph capture failed!");
    return false;
  }

  Serial.println("PHOTO CAPTURE SUCCESSFUL");
  Serial.printf("Image size: %u bytes\n", capturedImage->len);
  Serial.printf("Image dimensions: %u x %u\n",
                capturedImage->width, capturedImage->height);

  uint32_t imageSize = capturedImage->len;
  uint8_t sizeData[4] = {
    (uint8_t)(imageSize >> 24),
    (uint8_t)(imageSize >> 16),
    (uint8_t)(imageSize >> 8),
    (uint8_t)imageSize
  };
  imageInfoCharacteristic.setValue(sizeData, sizeof(sizeData));
  Serial.printf("IMAGE_INFO updated: %u bytes\n", imageSize);
  return true;
}

void setCommandValue(BLECharacteristic& characteristic,
                     const char* value,
                     size_t length) {
  characteristic.setValue(
      reinterpret_cast<const uint8_t*>(value), length);
}

void configureCharacteristics(BLEService& service) {
  commandCharacteristic = service.createCharacteristic(
    COMMAND_UUID,
    BLEProperty::Read | BLEProperty::Write,
    BLEPermission::ReadWriteOpen);
  setCommandValue(commandCharacteristic, "READY", 5);
    commandCharacteristic.onWrite([](BLECharacteristic& characteristic,
                                    const BLEConnInfo& connInfo) {
    String value = characteristic.getStringValue();

    Serial.print("Command received: ");
    Serial.println(value);

    if (value == "CAPTURE") {
      setCommandValue(characteristic, "CAPTURING", 9);;
      captureRequested = true;
    }
  });

  imageInfoCharacteristic = service.createCharacteristic(
    IMAGE_INFO_UUID, BLEProperty::Read, BLEPermission::ReadOpen);
  uint8_t initialSize[4] = {0, 0, 0, 0};
  imageInfoCharacteristic.setValue(initialSize, sizeof(initialSize));
  
  // Retained for protocol compatibility with the existing iPhone app.
  service.createCharacteristic(
    IMAGE_REQUEST_UUID,
    BLEProperty::Read | BLEProperty::Write,
    BLEPermission::ReadWriteOpen);
  service.createCharacteristic(
    IMAGE_DATA_UUID, BLEProperty::Read, BLEPermission::ReadOpen);

  imageStreamCharacteristic = service.createCharacteristic(
    IMAGE_STREAM_UUID, BLEProperty::Notify, BLEPermission::None);

  transferControlCharacteristic = service.createCharacteristic(
    TRANSFER_CONTROL_UUID, BLEProperty::Write, BLEPermission::WriteOpen);
  transferControlCharacteristic.onWrite([](BLECharacteristic& characteristic,
                                            const BLEConnInfo& connInfo) {
    size_t length = 0;
    const uint8_t* data = characteristic.getValue(&length);

    if (data == nullptr || length == 0) return;

    if (length == 5 && data[0] == 'S' && data[1] == 'T' &&
        data[2] == 'A' && data[3] == 'R' && data[4] == 'T') {
      transferStartRequested = true;
      return;
    }

    // ACK: ASCII 'A' followed by a 4-byte big-endian received-byte count.
    if (length == 5 && data[0] == 'A') {
      receivedAckOffset =
          ((uint32_t)data[1] << 24) |
          ((uint32_t)data[2] << 16) |
          ((uint32_t)data[3] << 8) |
          ((uint32_t)data[4]);
      ackReceived = true;
    }
  });
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting XIAO Camera BLE Server");
  Serial.printf("ESP32 Arduino core: %s\n", ESP_ARDUINO_VERSION_STR);
  Serial.println("Using built-in ESP32 Core 4 BLE API");
  Serial.println("Initialising OV3660 camera...");

  if (!initialiseCamera()) {
    Serial.println("WARNING: Camera unavailable.");
  } else {
    Serial.println("OV3660 ready.");
  }

  BTStatus bleStatus = BLE.begin(CAMERA_NAME);
  if (!bleStatus) {
    Serial.printf("BLE initialisation FAILED: %s\n", bleStatus.toString());
    return;
  }

  BTStatus mtuStatus = BLE.setMTU(517);
  if (!mtuStatus) {
    Serial.printf("BLE MTU request failed: %s\n", mtuStatus.toString());
  }

  bleServer = BLE.createServer();
  bleServer.advertiseOnDisconnect(true);
  bleServer.onDisconnect([](BLEServer& server,
                            const BLEConnInfo& connInfo,
                            uint8_t reason) {
    Serial.printf("iPhone disconnected. Reason: 0x%02X\n", reason);
    // Release the camera frame later from loop(), not on the BLE host task.
    disconnectCleanupRequested = true;
  });

  BLEService service = bleServer.createService(SERVICE_UUID);
  configureCharacteristics(service);

  BTStatus serverStatus = bleServer.start();
  if (!serverStatus) {
    Serial.printf("BLE GATT server start FAILED: %s\n", serverStatus.toString());
    return;
  }

  BLEAdvertising advertising = BLE.getAdvertising();
  advertising.setScanResponse(true);
  advertising.setName(CAMERA_NAME);
  advertising.addServiceUUID(SERVICE_UUID);

  BTStatus advertisingStatus = advertising.start();
  if (!advertisingStatus) {
    Serial.printf("BLE advertising FAILED: %s\n", advertisingStatus.toString());
    return;
  }

  Serial.println("BLE advertising started");
  Serial.println("BLE server started");
  Serial.printf("Advertising as %s\n", CAMERA_NAME);
}

void loop() {
  // Process requests raised by BLE callbacks on the Arduino task.
  if (disconnectCleanupRequested) {
    disconnectCleanupRequested = false;
    releaseCapturedImage();
  }

  if (captureRequested) {
    captureRequested = false;
    if (capturePhoto()) {
      setCommandValue(commandCharacteristic, "CAPTURED", 8);
      Serial.println("Capture completed successfully.");
    } else {
      setCommandValue(commandCharacteristic, "ERROR", 5);
      Serial.println("Capture failed.");
    }
  }

  if (transferStartRequested) {
    transferStartRequested = false;
    if (capturedImage == nullptr) {
      Serial.println("START rejected: no captured image.");
    } else {
      transferOffset = 0;
      lastAckOffset = 0;
      waitingForAck = false;
      transferActive = true;
      lastTransferTime = millis();
      Serial.println("Image transfer START received.");
      Serial.printf("Sending %u bytes.\n", capturedImage->len);
    }
  }

  if (ackReceived) {
    ackReceived = false;
    uint32_t ackOffset = receivedAckOffset;
    if (transferActive && waitingForAck &&
        ackOffset > lastAckOffset && ackOffset <= transferOffset) {
      lastAckOffset = ackOffset;
      waitingForAck = false;
      lastAckTime = millis();
      Serial.printf("ACK received: %u bytes\n", (unsigned int)ackOffset);
    }
  }

  if (!transferActive || capturedImage == nullptr) {
    delay(2);
    return;
  }

  if (millis() - lastTransferTime > TRANSFER_TIMEOUT_MS) {
    Serial.println("ERROR: Image transfer timed out.");
    releaseCapturedImage();
    return;
  }

  if (waitingForAck) {
    if (millis() - lastAckTime > ACK_TIMEOUT_MS) {
      Serial.println("ERROR: ACK timeout.");
      releaseCapturedImage();
    }
    delay(2);
    return;
  }

  if (transferOffset >= capturedImage->len) {
    Serial.println("Image transfer complete.");
    releaseCapturedImage();
    return;
  }

  size_t remaining = capturedImage->len - transferOffset;
  size_t chunkSize = remaining;
  if (chunkSize > CHUNK_SIZE) chunkSize = CHUNK_SIZE;

  BTStatus sent = imageStreamCharacteristic.notify(
    capturedImage->buf + transferOffset, chunkSize);

  if (sent) {
    transferOffset += chunkSize;
    lastTransferTime = millis();

    if (transferOffset - lastAckOffset >= ACK_WINDOW &&
        transferOffset < capturedImage->len) {
      waitingForAck = true;
      lastAckTime = millis();
      Serial.printf("Waiting for ACK at %u bytes\n",
                    (unsigned int)transferOffset);
    }
  } else {
    Serial.printf("BLE notification failed: %s\n", sent.toString());
    releaseCapturedImage();
    return;
  }

  delay(5);
}
