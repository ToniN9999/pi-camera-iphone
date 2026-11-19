
import os
import asyncio
from datetime import datetime
from pathlib import Path

from dbus_next import Message, Variant
from dbus_next.constants import MessageType

from bluez_peripheral.util import Adapter, get_message_bus
from bluez_peripheral.advert import Advertisement
from bluez_peripheral.gatt.service import Service
from bluez_peripheral.gatt.characteristic import characteristic
from bluez_peripheral.gatt.characteristic import CharacteristicFlags as CharFlags


SERVICE_UUID = "7f510001-1b15-4f0e-9b7b-8c8f8f8f0001"
COMMAND_UUID = "7f510002-1b15-4f0e-9b7b-8c8f8f8f0001"
IMAGE_INFO_UUID = "7f510003-1b15-4f0e-9b7b-8c8f8f8f0001"
IMAGE_REQUEST_UUID = "7f510004-1b15-4f0e-9b7b-8c8f8f8f0001"
IMAGE_DATA_UUID = "7f510005-1b15-4f0e-9b7b-8c8f8f8f0001"
IMAGE_STREAM_UUID = "7f510006-1b15-4f0e-9b7b-8c8f8f8f0001"
TRANSFER_CONTROL_UUID = "7f510007-1b15-4f0e-9b7b-8c8f8f8f0001"

DEVICE_NAME = os.uname().nodename
PROJECT_DIR = Path.home() / "camera_project"
CAPTURE_DIR = PROJECT_DIR / "captures"
JPEG_QUALITY = 85
CHUNK_SIZE = 180
NOTIFY_PAYLOAD_SIZE = 500  # Safe with the negotiated ATT MTU of 517
TRANSFER_WINDOW_SIZE = 40000
TRANSFER_ACK_TIMEOUT = 15


async def set_bluez_property(bus, adapter_path, name, value):
    reply = await bus.call(
        Message(
            destination="org.bluez",
            path=adapter_path,
            interface="org.freedesktop.DBus.Properties",
            member="Set",
            signature="ssv",
            body=["org.bluez.Adapter1", name, value],
        )
    )
    if reply.message_type == MessageType.ERROR:
        raise RuntimeError(f"Could not set {name}: {reply.body}")


async def get_bluez_property(bus, adapter_path, name):
    reply = await bus.call(
        Message(
            destination="org.bluez",
            path=adapter_path,
            interface="org.freedesktop.DBus.Properties",
            member="Get",
            signature="ss",
            body=["org.bluez.Adapter1", name],
        )
    )
    if reply.message_type == MessageType.ERROR:
        raise RuntimeError(f"Could not read {name}: {reply.body}")
    return reply.body[0].value


async def configure_pairing(bus, adapter_path):
    print("Setting Bluetooth PairableTimeout to 1 second...")
    await set_bluez_property(
        bus, adapter_path, "PairableTimeout", Variant("u", 1)
    )
    await asyncio.sleep(2)

    pairable = await get_bluez_property(bus, adapter_path, "Pairable")
    if pairable:
        await set_bluez_property(
            bus, adapter_path, "Pairable", Variant("b", False)
        )
        await asyncio.sleep(0.5)
        pairable = await get_bluez_property(bus, adapter_path, "Pairable")

    if pairable:
        raise RuntimeError("Bluetooth pairing is still enabled!")

    print("Bluetooth pairing disabled: Pairable = no")


class CameraService(Service):
    def __init__(self):
        self.last_command = b"READY"
        self.capture_task = None
        self.image_offset = 0
        self.transfer_task = None
        self.transfer_active = False
        self.expected_ack_offset = 0
        self.ack_offset = -1
        self.ack_event = asyncio.Event()
        super().__init__(SERVICE_UUID, True)

    def latest_image(self):
        files = list(CAPTURE_DIR.glob("*.jpg"))
        if not files:
            return None
        return max(files, key=lambda p: p.stat().st_mtime)

    @characteristic(COMMAND_UUID, CharFlags.READ | CharFlags.WRITE)
    def command(self, options):
        return self.last_command

    @command.setter
    def command(self, value, options):
        command = bytes(value).strip().upper()
        print("Bluetooth command received:", command.decode(errors="replace"))

        if command == b"CAPTURE":
            if self.capture_task is not None and not self.capture_task.done():
                self.last_command = b"BUSY"
                return

            self.last_command = b"CAPTURING"
            self.capture_task = asyncio.create_task(self.take_photo())
        else:
            self.last_command = b"UNKNOWN_COMMAND"

    async def take_photo(self):
        try:
            CAPTURE_DIR.mkdir(parents=True, exist_ok=True)
            filename = datetime.now().strftime("photo_%Y%m%d_%H%M%S.jpg")
            filepath = CAPTURE_DIR / filename

            print("Taking photograph:", filepath)

            process = await asyncio.create_subprocess_exec(
                "rpicam-still",
                "-n", "-t", "1000",
                "-q", str(JPEG_QUALITY),
                "-o", str(filepath),
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE,
            )
            _, stderr = await process.communicate()

            if process.returncode != 0:
                raise RuntimeError(stderr.decode(errors="replace"))

            if not filepath.exists() or filepath.stat().st_size == 0:
                raise RuntimeError("Camera produced an empty image.")

            print(
                f"Photograph saved: {filepath} "
                f"({filepath.stat().st_size / 1024:.1f} KB)"
            )
            self.last_command = b"CAPTURED"

        except Exception as error:
            print("Camera capture failed:", error)
            self.last_command = b"ERROR"
        finally:
            self.capture_task = None

    @characteristic(IMAGE_INFO_UUID, CharFlags.READ)
    def image_info(self, options):
        filepath = self.latest_image()
        if filepath is None:
            return (0).to_bytes(4, "big")
        return filepath.stat().st_size.to_bytes(4, "big")

    @characteristic(IMAGE_REQUEST_UUID, CharFlags.READ | CharFlags.WRITE)
    def image_request(self, options):
        return self.image_offset.to_bytes(4, "big")

    @image_request.setter
    def image_request(self, value, options):
        if len(value) != 4:
            print("Invalid image offset request.")
            return
        self.image_offset = int.from_bytes(value, "big")
        print("Image offset requested:", self.image_offset)

    @characteristic(IMAGE_DATA_UUID, CharFlags.READ)
    def image_data(self, options):
        filepath = self.latest_image()
        if filepath is None:
            return b""

        size = filepath.stat().st_size
        if self.image_offset >= size:
            return b""

        mtu = getattr(options, "mtu", 0) or 23
        payload_size = min(CHUNK_SIZE, max(1, mtu - 1))

        with filepath.open("rb") as image:
            image.seek(self.image_offset)
            data = image.read(payload_size)

        return data


    @characteristic(IMAGE_STREAM_UUID, CharFlags.NOTIFY)
    def image_stream(self, options):
        # Notifications are sent by calling image_stream.changed(bytes).
        pass

    @characteristic(TRANSFER_CONTROL_UUID, CharFlags.WRITE)
    def transfer_control(self, options):
        # Write-only control point: START or b"A" + 4-byte acknowledged offset.
        pass

    @transfer_control.setter
    def transfer_control(self, value, options):
        message = bytes(value)
        if message == b"START":
            if self.transfer_task is not None and not self.transfer_task.done():
                print("Image transfer request ignored: transfer already active.")
                return
            self.transfer_task = asyncio.create_task(self.stream_latest_image())
            return

        if len(message) == 5 and message[:1] == b"A":
            acknowledged = int.from_bytes(message[1:], "big")
            if self.transfer_active and acknowledged == self.expected_ack_offset:
                self.ack_offset = acknowledged
                self.ack_event.set()
            else:
                print("Unexpected image transfer acknowledgement:", acknowledged)
            return

        print("Invalid transfer control message:", message)

    async def stream_latest_image(self):
        self.transfer_active = True
        self.last_command = b"TRANSFERRING"
        try:
            filepath = self.latest_image()
            if filepath is None:
                raise RuntimeError("No photograph is available to transfer.")

            image_data = filepath.read_bytes()
            total = len(image_data)
            print(f"Starting notification transfer: {total} bytes")
            offset = 0

            while offset < total:
                batch_end = min(offset + TRANSFER_WINDOW_SIZE, total)
                self.expected_ack_offset = batch_end
                self.ack_offset = -1
                self.ack_event.clear()

                while offset < batch_end:
                    chunk = image_data[offset:offset + NOTIFY_PAYLOAD_SIZE]
                    self.image_stream.changed(chunk)
                    offset += len(chunk)
                    # Yield to BlueZ so queued notifications can be delivered.
                    await asyncio.sleep(0.002)

                if offset < total:
                    await asyncio.wait_for(
                        self.ack_event.wait(), timeout=TRANSFER_ACK_TIMEOUT
                    )
                    if self.ack_offset != offset:
                        raise RuntimeError(
                            f"Transfer acknowledgement mismatch at byte {offset}."
                        )

            print(f"Notification transfer complete: {total} bytes")
            self.last_command = b"TRANSFERRED"

        except asyncio.TimeoutError:
            print("Image transfer stopped: acknowledgement timed out.")
            self.last_command = b"TRANSFER_ERROR"
        except Exception as error:
            print("Image transfer failed:", error)
            self.last_command = b"TRANSFER_ERROR"
        finally:
            self.transfer_active = False
            self.transfer_task = None


async def main():
    print("Starting Pi Camera BLE server...")
    bus = await get_message_bus()
    adapter_path = "/org/bluez/hci0"

    introspection = await bus.introspect("org.bluez", adapter_path)
    proxy = bus.get_proxy_object("org.bluez", adapter_path, introspection)
    adapter = Adapter(proxy)

    print("Bluetooth adapter:", await adapter.get_address())
    print("Adapter name:", await adapter.get_alias())

    await configure_pairing(bus, adapter_path)

    service = CameraService()
    await service.register(bus, adapter=adapter)

    advert = Advertisement(
        DEVICE_NAME, [SERVICE_UUID], appearance=0, timeout=3600
    )
    await advert.register(bus, adapter=adapter)

    print("----------------------------------")
    print("Pi Camera BLE server is running")
    print("JPEG quality:", JPEG_QUALITY)
    print("Image chunk maximum:", CHUNK_SIZE)
    print("Notification payload:", NOTIFY_PAYLOAD_SIZE)
    print("Negotiated MTU observed in btmon: 517")
    print("Notification window:", TRANSFER_WINDOW_SIZE)
    print("Pairable: no")
    print("Waiting for connections...")
    print("----------------------------------")

    try:
        while True:
            await asyncio.sleep(1)
    finally:
        await service.unregister()


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\nBLE server stopped.")
