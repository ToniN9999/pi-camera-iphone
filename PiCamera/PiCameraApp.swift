import SwiftUI

@main
struct PiCameraApp: App {
    @StateObject private var camera = CameraBluetoothManager()

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(camera)
        }
    }
}

struct ContentView: View {
    @EnvironmentObject private var camera: CameraBluetoothManager

    var body: some View {
        NavigationStack {
            VStack(spacing: 18) {
                Image(systemName: "camera.fill")
                    .font(.system(size: 42))
                    .foregroundStyle(.blue)
                    .padding(.top, 18)

                Text("Pi Camera")
                    .font(.largeTitle.bold())

                Text(camera.status)
                    .font(.callout)
                    .foregroundStyle(.secondary)
                    .multilineTextAlignment(.center)
                    .frame(maxWidth: .infinity)
                    .padding()
                    .background(.quaternary.opacity(0.5), in: RoundedRectangle(cornerRadius: 12))
                    if camera.isConnected || camera.isConnecting {
                        Button {
                           camera.returnToCameraList()
                        } label: {
                           Label("Back to Camera List", systemImage: "arrow.left")
                                 .frame(maxWidth: .infinity)
                        }
                        .buttonStyle(.bordered)
                    }

                Button {
                    camera.connect()
                } label: {
                    Label(camera.isScanning ? "Searching for cameras…" : "Find Cameras",
                          systemImage: camera.isScanning ? "dot.radiowaves.left.and.right" : "magnifyingglass")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.borderedProminent)
                .disabled(camera.isConnected || camera.isConnecting || camera.isScanning)

                if camera.isScanning {
                    HStack {
                        ProgressView()
                        Text("Scanning for cameras…")
                            .font(.caption)
                            .foregroundStyle(.secondary)
                        Spacer()
                        Button("Stop") { camera.stopScanning() }
                            .font(.caption)
                    }
                }

                if !camera.discoveredCameras.isEmpty && !camera.isConnected {
                    VStack(alignment: .leading, spacing: 8) {
                        Text("Available Cameras")
                            .font(.headline)
                        ForEach(camera.discoveredCameras) { device in
                            Button {
                                camera.connect(to: device)
                            } label: {
                                HStack {
                                    Image(systemName: "camera.fill")
                                        .foregroundStyle(.blue)
                                    VStack(alignment: .leading, spacing: 2) {
                                        Text(device.name)
                                            .font(.body.weight(.medium))
                                        Text("Signal: \(device.rssi) dBm")
                                            .font(.caption)
                                            .foregroundStyle(.secondary)
                                    }
                                    Spacer()
                                    Image(systemName: "chevron.right")
                                        .foregroundStyle(.tertiary)
                                }
                                .contentShape(Rectangle())
                            }
                            .buttonStyle(.plain)
                            .padding(.vertical, 6)
                            Divider()
                        }
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                }

                Button {
                    camera.takePhoto()
                } label: {
                    Label("Take Photograph", systemImage: "camera shutter.button")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.borderedProminent)
                .tint(.indigo)
                .disabled(!camera.isConnected || camera.isBusy)

                if camera.isBusy {
                    ProgressView(value: camera.progress)
                    Text(camera.progressLabel)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                }

                if let image = camera.image {
                    Image(uiImage: image)
                        .resizable()
                        .scaledToFit()
                        .clipShape(RoundedRectangle(cornerRadius: 12))
                        .frame(maxHeight: 360)

                    Button {
                        camera.saveToPhotos()
                    } label: {
                        Label("Save to Photos", systemImage: "square.and.arrow.down")
                            .frame(maxWidth: .infinity)
                    }
                    .buttonStyle(.bordered)
                }

                Spacer(minLength: 0)
            }
            .padding()
            .navigationBarTitleDisplayMode(.inline)
        }
        .alert("Pi Camera", isPresented: $camera.showAlert) {
            Button("OK", role: .cancel) { }
        } message: {
            Text(camera.alertMessage)
        }
    }
}
