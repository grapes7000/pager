# Pager

ESP32-based wireless hardware messenger.

## Prototype hardware

- ESP32-WROOM dev board
- 128x64 SH1106 I2C OLED at 0x3C
- KY-040 rotary encoder
- BLE HID keyboard (tested: 518BT / `Bluetooth Keyboard`)

| Device | ESP32 |
| --- | --- |
| OLED SDA | GPIO21 |
| OLED SCL/SCK | GPIO22 |
| OLED VCC | 3V3 |
| OLED GND | GND |
| KY-040 CLK | GPIO34 |
| KY-040 DT | GPIO35 |
| KY-040 SW | GPIO32 |
| KY-040 + | 3V3 |
| KY-040 GND | GND |

GPIO34/35 are input-only and have no internal pull resistors; this prototype relies on the KY-040 module's pull-ups.

## Current UI

The encoder scrolls a three-message inbox viewport. Unread messages are marked with `*`. When selection rests on an unread message for 900 ms, it is marked read and the unread counter decreases.

The UI consumes a transport-independent `Message` model so Bluetooth keyboard, Wi-Fi, nRF24, or relay transports can be added without rewriting the display layer.

## BLE keyboard setup and pairing

The keyboard is **BLE HID / HID over GATT (HOGP)**, not Bluetooth Classic HID. This distinction matters: using a Classic HID host causes SDP failures and Classic inquiry may never see the keyboard.

The working Linux identification procedure was:

```bash
bluetoothctl
scan on
```

Put the keyboard into pairing/discoverable mode. When it appears, note its address, then inspect it:

```text
info E8:74:76:2D:CF:DB
```

For the tested 518BT keyboard, BlueZ reported:

```text
Name: Bluetooth Keyboard
Address: E8:74:76:2D:CF:DB
Address type: random
Appearance: 0x03c1
Icon: input-keyboard
LegacyPairing: no
UUID: Human Interface Device (00001812-0000-1000-8000-00805f9b34fb)
```

The important proof is service UUID **0x1812**, the BLE Human Interface Device service. The firmware therefore uses **NimBLE-Arduino as a BLE central/client** and subscribes to HID report notifications. Do not use `ESP32-BLE-Keyboard`: that library makes the ESP32 act as a keyboard peripheral, which is the opposite direction.

### Adding another keyboard/device

Do **not** repeat the Classic-vs-BLE debugging process. Identify the new device with `bluetoothctl` first:

1. Run `bluetoothctl`, then `scan on`.
2. Put only the keyboard you want to add into pairing mode.
3. Copy its Bluetooth address exactly.
4. Run `info <ADDRESS>`.
5. Confirm it exposes `00001812-0000-1000-8000-00805f9b34fb` / Human Interface Device.
6. Update `kKeyboardAddress` in `src/ble_keyboard_host.cpp` to that address.
7. Build, upload, and monitor. Put the keyboard back into pairing mode while Pager scans.

The firmware intentionally matches the **exact known address**, not every device advertising HID service 0x1812. During testing another nearby device, `KS03~EE01BE`, also advertised 0x1812 and Pager connected to it first. Exact-address matching prevents that.

Expected successful serial path:

```text
Starting BLE HID keyboard discovery...
[BLE] starting BLE HID keyboard client...
[BLE] scanning for HID service 1812 / known keyboard...
[BLE] seen: e8:74:76:2d:cf:db name=Bluetooth Keyboard
[BLE] keyboard found: e8:74:76:2d:cf:db HID=1812
[BLE] connecting to e8:74:76:2d:cf:db...
[BLE] connected; discovering HID service...
```

If the address appears as `(random)` in BlueZ, that is normal for this keyboard. Use the exact address discovered for that device. If a future keyboard does **not** expose UUID 0x1812, do not assume this BLE-HID implementation supports it; identify its transport/profile before changing the firmware.

## Build

This repo deliberately uses its own Python 3.13 virtual environment and PlatformIO 6.1.18. Do not modify or downgrade the system PlatformIO/Python installation for this project.

```bash
git pull
.pio-venv/bin/pio run
.pio-venv/bin/pio run -t upload
.pio-venv/bin/pio device monitor -b 115200
```

The configured flash layout is 2 MB and uses `partitions.csv` (one application slot, no OTA update slot).

Press the encoder button to compose, then type using the BLE keyboard or serial monitor. Enter adds the message to the local inbox and logs it over serial; wireless message delivery is not implemented. Escape or another encoder press cancels.

Run the host regression checks (requires Bash and g++) with:

```bash
bash tests/run_host_tests.sh
```

These check composer boundaries, message capacity, selection/read timing, and button debounce using simulated time, pins, and display calls. BLE pairing/reconnection and physical encoder/display behavior still require board testing.
