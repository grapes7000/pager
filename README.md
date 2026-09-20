# Pager

ESP32-based two-way wireless hardware messenger.

## Prototype hardware

- ESP32-WROOM dev board
- 128x64 SH1106 I2C OLED at 0x3C
- KY-040 rotary encoder
- BLE HID keyboard
- ESP-NOW direct pager-to-pager link

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

## Messaging

The two pagers now discover each other directly over **ESP-NOW** on Wi-Fi channel 6. No router, hotspot, SSID, password, or internet connection is required.

Each pager broadcasts a small discovery packet every two seconds. Once it hears the other logical pager ID, it learns that ESP32's Wi-Fi MAC and adds it as an ESP-NOW unicast peer. Text messages are then sent directly to that peer.

The application protocol provides:

- fixed logical identities (`Pager1` and `Pager2`)
- automatic peer discovery; no hard-coded ESP32 Wi-Fi MAC addresses
- 160-character message payloads
- application-level ACKs
- retry after a lost ACK or temporarily unavailable peer
- an eight-message outbound queue
- duplicate suppression using boot-session ID + message ID
- malformed packet validation before anything reaches the UI

If one pager is off, composed messages stay queued in RAM. When the other pager comes back and discovery resumes, the oldest pending message is retried until its ACK arrives. The current queue is volatile, so pending messages do not survive a reboot yet.

**Security note:** the current pager-to-pager ESP-NOW transport is not application-encrypted. ESP-NOW frames are unencrypted in this first direct-link implementation. Add authenticated encryption before treating radio traffic as private.

Useful serial messages:

```text
[LINK] ready id=1 name=Pager1 channel=6 mac=...
[LINK] peer discovered: ...
[LINK] queued message 1; pending=1
[LINK] sent message 1; awaiting ACK
[LINK] delivered message 1
[LINK] received message 1 (... bytes)
```

## UI

The encoder scrolls a three-message inbox viewport. Unread messages are marked with `*`. When selection rests on an unread message for 900 ms, it is marked read and the unread counter decreases.

Press the encoder button to compose. Type with the pager's BLE keyboard and press Enter to send. Escape or another encoder press cancels. Incoming ESP-NOW messages are added to the inbox as unread.

The old seeded demo messages have been removed; a freshly flashed pager starts with an empty inbox.

## BLE keyboards

Both build environments include the hardened BLE-HID reconnect behavior. If a configured keyboard is off or out of range, its pager keeps scanning and automatically reconnects when the keyboard returns.

- `pager1`: original `Bluetooth Keyboard`, matching the known `E8:74:76:2D:CF:D*` address range.
- `pager2`: K808 at `41:83:6B:99:22:04`.

The keyboards use BLE HID / HID over GATT (service UUID `0x1812`). NimBLE-Arduino acts as the BLE central/client. `ESP32-BLE-Keyboard` is the wrong direction for this project because it makes the ESP32 itself a keyboard peripheral.

## Build and flash

This repo deliberately uses its own Python 3.13 virtual environment and PlatformIO 6.1.18. Do not modify or downgrade the system PlatformIO/Python installation for this project.

Pager #1:

```bash
.pio-venv/bin/pio run -e pager1 -t upload --upload-port /dev/ttyUSB0
```

Pager #2:

```bash
.pio-venv/bin/pio run -e pager2 -t upload --upload-port /dev/ttyUSB0
```

Serial monitor:

```bash
.pio-venv/bin/pio device monitor -p /dev/ttyUSB0 -b 115200
```

The configured flash layout is 2 MB and uses `partitions.csv` (one application slot, no OTA update slot).

## Tests

Run the host regression checks with:

```bash
bash tests/run_host_tests.sh
```

These cover the existing composer, message capacity, selection/read timing, and encoder debounce logic. ESP-NOW delivery/retry and BLE pairing/reconnection require hardware testing on the two physical pagers.
