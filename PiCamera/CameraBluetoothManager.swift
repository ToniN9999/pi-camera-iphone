import Foundation
import Combine
import CoreBluetooth
import Photos
import UIKit

struct DiscoveredCamera: Identifiable {
    let peripheral: CBPeripheral
    let name: String
    let rssi: Int

    var id: UUID { peripheral.identifier }
}

@MainActor
final class CameraBluetoothManager: NSObject, ObservableObject {
    // These UUIDs match the working Raspberry Pi BLE server.
    private let serviceUUID = CBUUID(string: "7f510001-1b15-4f0e-9b7b-8c8f8f8f0001")
    private let commandUUID = CBUUID(string: "7f510002-1b15-4f0e-9b7b-8c8f8f8f0001")
    private let imageInfoUUID = CBUUID(string: "7f510003-1b15-4f0e-9b7b-8c8f8f8f0001")
    private let imageStreamUUID = CBUUID(string: "7f510006-1b15-4f0e-9b7b-8c8f8f8f0001")
    private let transferControlUUID = CBUUID(string: "7f510007-1b15-4f0e-9b7b-8c8f8f8f0001")

    private let ackWindow = 40_000

    @Published var status = "Bluetooth is starting…"
    @Published var isConnected = false
    @Published var isConnecting = false
    @Published var isScanning = false
    @Published var discoveredCameras: [DiscoveredCamera] = []
    @Published var selectedCameraName: String?
    @Published var isBusy = false
    @Published var progress = 0.0
    @Published var progressLabel = ""
    @Published var image: UIImage?
    @Published var showAlert = false
    @Published var alertMessage = ""

    private var central: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var commandCharacteristic: CBCharacteristic?
    private var infoCharacteristic: CBCharacteristic?
    private var streamCharacteristic: CBCharacteristic?
    private var controlCharacteristic: CBCharacteristic?

    private var expectedImageSize = 0
    private var receivedData = Data()
    private var nextAck = 40_000
    private var waitingForCapture = false
    private var capturePolls = 0
    private var transferActive = false

    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: .main)
    }

    func connect() {
        guard central.state == .poweredOn else {
            status = "Please turn on Bluetooth on the iPhone."
            return
        }
        guard !isScanning, !isConnecting, !isConnected else { return }

        discoveredCameras.removeAll()
        selectedCameraName = nil
        isScanning = true
        status = "Searching for Pi cameras…"
        central.scanForPeripherals(withServices: [serviceUUID], options: [
            CBCentralManagerScanOptionAllowDuplicatesKey: false
        ])
    }

    func stopScanning() {
        guard isScanning else { return }
        central.stopScan()
        isScanning = false
        status = discoveredCameras.isEmpty
            ? "Scan stopped. No cameras found."
            : "Select a camera from the list."
    }

    func connect(to camera: DiscoveredCamera) {
        guard central.state == .poweredOn, !isConnecting, !isConnected else { return }
        central.stopScan()
        isScanning = false
        peripheral = camera.peripheral
        selectedCameraName = camera.name
        commandCharacteristic = nil
        infoCharacteristic = nil
        streamCharacteristic = nil
        controlCharacteristic = nil
        peripheral?.delegate = self
        isConnecting = true
        status = "Connecting to \(camera.name)…"
        central.connect(camera.peripheral, options: nil)
    }

    func takePhoto() {
        guard let peripheral, let commandCharacteristic else { return }
        isBusy = true
        progress = 0
        progressLabel = "Capturing photograph…"
        status = "Sending capture command…"
        waitingForCapture = true
        capturePolls = 0
        peripheral.writeValue(Data("CAPTURE".utf8), for: commandCharacteristic, type: .withResponse)
    }

    private func pollCaptureStatus() {
        guard waitingForCapture, let peripheral, let commandCharacteristic else { return }
        capturePolls += 1
        if capturePolls > 60 {
            waitingForCapture = false
            finishWithError("Timed out waiting for the camera.")
            return
        }
        peripheral.readValue(for: commandCharacteristic)
        DispatchQueue.main.asyncAfter(deadline: .now() + 0.5) { [weak self] in
            guard let self, self.waitingForCapture else { return }
            self.pollCaptureStatus()
        }
    }

    private func beginImageTransfer() {
        guard let peripheral, let infoCharacteristic else { return }
        status = "Checking photograph size…"
        peripheral.readValue(for: infoCharacteristic)
    }

    private func startStreaming() {
        guard let peripheral, let streamCharacteristic, let controlCharacteristic else { return }
        guard expectedImageSize > 0 else {
            finishWithError("The Pi has no photograph available.")
            return
        }

        receivedData.removeAll(keepingCapacity: true)
        receivedData.reserveCapacity(expectedImageSize)
        nextAck = ackWindow
        transferActive = true
        progress = 0
        progressLabel = "Receiving photograph…"
        status = "Starting Bluetooth transfer…"
        peripheral.setNotifyValue(true, for: streamCharacteristic)

        DispatchQueue.main.asyncAfter(deadline: .now() + 180) { [weak self] in
            guard let self, self.transferActive else { return }
            self.finishWithError("Image transfer timed out. Please try again.")
        }
    }

    private func acknowledgeIfNeeded() {
        guard let peripheral, let controlCharacteristic else { return }
        let count = receivedData.count
        if count >= nextAck && count < expectedImageSize {
            var ack = Data([0x41]) // ASCII "A"
            var value = UInt32(count).bigEndian
            withUnsafeBytes(of: &value) { ack.append(contentsOf: $0) }
            nextAck += ackWindow
            peripheral.writeValue(ack, for: controlCharacteristic, type: .withResponse)
        }
    }

    private func completeTransfer() {
        guard transferActive else { return }
        transferActive = false
        isBusy = false
        progress = 1
        progressLabel = "Transfer complete"
        guard receivedData.count == expectedImageSize,
              let decoded = UIImage(data: receivedData) else {
            finishWithError("The received photograph is incomplete or invalid.")
            return
        }
        image = decoded
        status = "Photograph received successfully."
        if let peripheral, let streamCharacteristic {
            peripheral.setNotifyValue(false, for: streamCharacteristic)
        }
    }

    private func finishWithError(_ message: String) {
        waitingForCapture = false
        transferActive = false
        isBusy = false
        status = message
        alertMessage = message
        showAlert = true
        if let peripheral, let streamCharacteristic {
            peripheral.setNotifyValue(false, for: streamCharacteristic)
        }
    }

    func saveToPhotos() {
        guard let image else { return }
        PHPhotoLibrary.requestAuthorization(for: .addOnly) { [weak self] permission in
            guard let self else { return }
            guard permission == .authorized || permission == .limited else {
                Task { @MainActor in self.finishWithError("Permission to add photographs to Photos was not granted.") }
                return
            }
            UIImageWriteToSavedPhotosAlbum(image, nil, nil, nil)
            Task { @MainActor in self.status = "Photograph saved to iPhone Photos." }
        }
    }
}

extension CameraBluetoothManager: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            status = "Ready. Find a Pi camera to connect."
        case .poweredOff:
            status = "Bluetooth is switched off."
        case .unauthorized:
            status = "Bluetooth permission is not available."
        case .unsupported:
            status = "This iPhone does not support Bluetooth LE."
        default:
            status = "Bluetooth is unavailable."
        }
    }

    func centralManager(_ central: CBCentralManager,
                        didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any],
                        rssi RSSI: NSNumber) {
        guard isScanning else { return }
        let advertisedName = advertisementData[CBAdvertisementDataLocalNameKey] as? String
        let cleanedAdvertisedName = advertisedName?.trimmingCharacters(in: .whitespacesAndNewlines)
        let name: String
        if let cleanedAdvertisedName, !cleanedAdvertisedName.isEmpty {
            name = cleanedAdvertisedName
        } else {
            name = peripheral.name ?? "Unnamed Pi camera"
        }

        let camera = DiscoveredCamera(peripheral: peripheral, name: name, rssi: RSSI.intValue)
        if let index = discoveredCameras.firstIndex(where: { $0.id == peripheral.identifier }) {
            discoveredCameras[index] = camera
        } else {
            discoveredCameras.append(camera)
        }
        discoveredCameras.sort { $0.name.localizedCaseInsensitiveCompare($1.name) == .orderedAscending }
        status = "Found \(discoveredCameras.count) camera(s). Select one to connect."
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        status = "Connected. Discovering camera service…"
        peripheral.discoverServices([serviceUUID])
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        isConnecting = false
        self.peripheral = nil
        status = "Connection to \(selectedCameraName ?? "camera") failed: \(error?.localizedDescription ?? "Unknown error")"
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        isConnected = false
        isConnecting = false
        isBusy = false
        waitingForCapture = false
        transferActive = false
        self.peripheral = nil
        commandCharacteristic = nil
        infoCharacteristic = nil
        streamCharacteristic = nil
        controlCharacteristic = nil
        status = "Bluetooth disconnected. Find a camera to reconnect."
    }
}

extension CameraBluetoothManager: CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        if let error { finishWithError(error.localizedDescription); return }
        guard let service = peripheral.services?.first(where: { $0.uuid == serviceUUID }) else {
            finishWithError("Camera service was not found.")
            return
        }
        peripheral.discoverCharacteristics(
            [commandUUID, imageInfoUUID, imageStreamUUID, transferControlUUID],
            for: service
        )
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didDiscoverCharacteristicsFor service: CBService,
                    error: Error?) {
        if let error { finishWithError(error.localizedDescription); return }
        for characteristic in service.characteristics ?? [] {
            switch characteristic.uuid {
            case commandUUID: commandCharacteristic = characteristic
            case imageInfoUUID: infoCharacteristic = characteristic
            case imageStreamUUID: streamCharacteristic = characteristic
            case transferControlUUID: controlCharacteristic = characteristic
            default: break
            }
        }
        guard commandCharacteristic != nil, infoCharacteristic != nil,
              streamCharacteristic != nil, controlCharacteristic != nil else {
            finishWithError("One or more required Bluetooth characteristics are missing.")
            return
        }
        isConnected = true
        isConnecting = false
        status = "Connected to \(selectedCameraName ?? "Pi camera")."
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didWriteValueFor characteristic: CBCharacteristic,
                    error: Error?) {
        if let error { finishWithError("Bluetooth write failed: \(error.localizedDescription)"); return }
        if characteristic.uuid == commandUUID && waitingForCapture {
            pollCaptureStatus()
        }
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didUpdateValueFor characteristic: CBCharacteristic,
                    error: Error?) {
        if let error { finishWithError("Bluetooth read failed: \(error.localizedDescription)"); return }
        guard let value = characteristic.value else { return }

        if characteristic.uuid == commandUUID && waitingForCapture {
            let cameraState = String(data: value, encoding: .utf8) ?? ""
            switch cameraState {
            case "CAPTURED":
                waitingForCapture = false
                beginImageTransfer()
            case "ERROR":
                finishWithError("The Raspberry Pi reported a camera error.")
            case "BUSY":
                finishWithError("The Raspberry Pi camera is busy.")
            default:
                status = "Camera: \(cameraState)"
            }
        } else if characteristic.uuid == imageInfoUUID {
            guard value.count == 4 else {
                finishWithError("The Pi returned an invalid image size.")
                return
            }
            expectedImageSize = value.reduce(0) { ($0 << 8) | Int($1) }
            startStreaming()
        } else if characteristic.uuid == imageStreamUUID && transferActive {
            receivedData.append(value)
            let count = receivedData.count
            progress = min(1, Double(count) / Double(max(expectedImageSize, 1)))
            progressLabel = "\(Int(progress * 100))%  (\(count) / \(expectedImageSize) bytes)"
            acknowledgeIfNeeded()
            if count == expectedImageSize {
                completeTransfer()
            } else if count > expectedImageSize {
                finishWithError("Received more data than expected.")
            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didUpdateNotificationStateFor characteristic: CBCharacteristic,
                    error: Error?) {

        if let error {
            finishWithError("Could not enable image notifications: \(error.localizedDescription)")
            return
        }

        guard characteristic.uuid == imageStreamUUID else { return }

        guard characteristic.isNotifying else {
            return
        }

        guard let controlCharacteristic else {
            finishWithError("Transfer control characteristic not found.")
            return
        }

        status = "Starting image transfer..."

        let startCommand = Data("START".utf8)

        peripheral.writeValue(
            startCommand,
            for: controlCharacteristic,
            type: .withResponse
        )
    }
}
