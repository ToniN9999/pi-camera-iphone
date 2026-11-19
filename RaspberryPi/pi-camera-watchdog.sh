#!/bin/bash

ACTIVE=$(bluetoothctl show | awk '/ActiveInstances:/ {print $2}')

if [ -z "$ACTIVE" ] || [ "$ACTIVE" = "0x00" ]; then
    logger -t pi-camera-watchdog "Bluetooth advertising missing. Restarting camera BLE service."
    systemctl restart pi-camera-ble.service
fi
