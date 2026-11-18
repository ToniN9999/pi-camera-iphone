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

                Button {
                    camera.connect()
                } label: {
                    Label(camera.isConnected ? "Connected to POS-URINE-AN" : "Connect to POS-URINE-AN",
                          systemImage: camera.isConnected ? "checkmark.circle.fill" : "dot.radiowaves.left.and.right")
                        .frame(maxWidth: .infinity)
                }
                .buttonStyle(.borderedProminent)
                .disabled(camera.isConnected || camera.isConnecting)

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
