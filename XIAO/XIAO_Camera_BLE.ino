#include <NimBLEDevice.h>
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

NimBLECharacteristic* commandCharacteristic = nullptr;
NimBLECharacteristic* imageInfoCharacteristic = nullptr;
NimBLECharacteristic* imageStreamCharacteristic = nullptr;
NimBLECharacteristic* transferControlCharacteristic = nullptr;

camera_fb_t* capturedImage = nullptr;

volatile bool transferActive = false;
volatile bool waitingForAck = false;

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
  config.frame_size = FRAMESIZE_QVGA;
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
  imageInfoCharacteristic->setValue(sizeData, sizeof(sizeData));
  Serial.printf("IMAGE_INFO updated: %u bytes\n", imageSize);
  return true;
}

class CommandCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* characteristic,
               NimBLEConnInfo& connInfo) override {
    std::string value = characteristic->getValue();
    Serial.print("Command received: ");
    Serial.println(value.c_str());

    if (value == "CAPTURE") {
      characteristic->setValue("CAPTURING");
      if (capturePhoto()) {
        characteristic->setValue("CAPTURED");
        Serial.println("Capture completed successfully.");
      } else {
        characteristic->setValue("ERROR");
        Serial.println("Capture failed.");
      }
    }
  }
};

class TransferControlCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* characteristic,
               NimBLEConnInfo& connInfo) override {
    std::string value = characteristic->getValue();

    if (value == "START") {
      if (capturedImage == nullptr) {
        Serial.println("START rejected: no captured image.");
        return;
      }
      transferOffset = 0;
      lastAckOffset = 0;
      waitingForAck = false;
      transferActive = true;
      lastTransferTime = millis();
      Serial.println("Image transfer START received.");
      Serial.printf("Sending %u bytes.\n", capturedImage->len);
      return;
    }

    // ACK: ASCII 'A' followed by a 4-byte big-endian received-byte count.
    if (value.size() == 5 && value[0] == 'A') {
      uint32_t ackOffset =
          ((uint32_t)(uint8_t)value[1] << 24) |
          ((uint32_t)(uint8_t)value[2] << 16) |
          ((uint32_t)(uint8_t)value[3] << 8) |
          ((uint32_t)(uint8_t)value[4]);

      if (transferActive && waitingForAck &&
          ackOffset > lastAckOffset && ackOffset <= transferOffset) {
        lastAckOffset = ackOffset;
        waitingForAck = false;
        lastAckTime = millis();
        Serial.printf("ACK received: %u bytes\n", ackOffset);
      }
    }
  }
};

class ServerCallbacks : public NimBLEServerCallbacks {
  void onDisconnect(NimBLEServer* server,
                    NimBLEConnInfo& connInfo,
                    int reason) override {
    Serial.print("iPhone disconnected. Reason: ");
    Serial.println(reason);
    releaseCapturedImage();
    NimBLEDevice::startAdvertising();
    Serial.println("Restarting BLE advertising");
  }
};

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting XIAO Camera BLE Server");
  Serial.println("Initialising OV3660 camera...");

  if (!initialiseCamera()) {
    Serial.println("WARNING: Camera unavailable.");
  } else {
    Serial.println("OV3660 ready.");
  }

  NimBLEDevice::init(CAMERA_NAME);
  NimBLEDevice::setMTU(517);

  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  NimBLEService* service = server->createService(SERVICE_UUID);

  commandCharacteristic = service->createCharacteristic(
    COMMAND_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  commandCharacteristic->setValue("READY");
  commandCharacteristic->setCallbacks(new CommandCallbacks());

  imageInfoCharacteristic = service->createCharacteristic(
    IMAGE_INFO_UUID, NIMBLE_PROPERTY::READ);
  uint8_t initialSize[4] = {0, 0, 0, 0};
  imageInfoCharacteristic->setValue(initialSize, sizeof(initialSize));

  // Retained for protocol compatibility.
  service->createCharacteristic(
    IMAGE_REQUEST_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
  service->createCharacteristic(IMAGE_DATA_UUID, NIMBLE_PROPERTY::READ);

  imageStreamCharacteristic = service->createCharacteristic(
    IMAGE_STREAM_UUID, NIMBLE_PROPERTY::NOTIFY);
  transferControlCharacteristic = service->createCharacteristic(
    TRANSFER_CONTROL_UUID, NIMBLE_PROPERTY::WRITE);
  transferControlCharacteristic->setCallbacks(new TransferControlCallbacks());

  service->start();

  NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();

  // Keep the 128-bit service UUID in the main 31-byte advertisement.
  // Put the configurable device name in the scan response so longer
  // names do not overflow the legacy BLE advertisement packet.
  NimBLEAdvertisementData advertisementData;
  advertisementData.addServiceUUID(SERVICE_UUID);

  NimBLEAdvertisementData scanResponseData;
  scanResponseData.setName(CAMERA_NAME);

  advertising->enableScanResponse(true);

  if (!advertising->setAdvertisementData(advertisementData)) {
    Serial.println("ERROR: Failed to set BLE advertisement data.");
  }
  if (!advertising->setScanResponseData(scanResponseData)) {
    Serial.println("ERROR: Failed to set BLE scan response data.");
  }

  advertising->start();

  Serial.println("BLE advertising started");
  Serial.println("BLE server started");
  Serial.printf("Advertising as %s\n", CAMERA_NAME);
}

void loop() {
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

  imageStreamCharacteristic->setValue(
    capturedImage->buf + transferOffset, chunkSize);
  bool sent = imageStreamCharacteristic->notify();

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
    Serial.println("BLE notification failed.");
    releaseCapturedImage();
    return;
  }

  delay(5);
}
