# Pi Camera iPhone app

Native SwiftUI/Core Bluetooth client for the existing Raspberry Pi BLE camera server.

## Current implementation

- Scans for the advertised service and connects to `POS-URINE-AN`.
- Sends `CAPTURE` to characteristic `0002`.
- Polls capture status until `CAPTURED`.
- Reads the JPEG byte count from characteristic `0003`.
- Enables notifications on characteristic `0006`.
- Sends `START` and acknowledges each 10,000-byte window using characteristic `0007`.
- Displays the received JPEG and can save it to Photos.

The app uses the existing Raspberry Pi service and characteristic UUIDs. It does not use Wi-Fi, a web page, Cloudflare, or an internet connection at runtime.

## Build

GitHub Actions builds an unsigned IPA on a macOS runner. The IPA must then be signed for the iPhone using a sideloading tool and an Apple ID. A free Apple Personal Team provisioning profile typically needs renewal every 7 days.

## Important

This is the first development build. Test against the Pi before relying on it. The Raspberry Pi server is not changed by this project.
