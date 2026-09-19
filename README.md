# Pager

ESP32-based wireless hardware messenger.

## Prototype hardware

- ESP32-WROOM dev board
- 128x64 SH1106 I2C OLED at 0x3C
- KY-040 rotary encoder

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

## Build

```bash
pio run
pio run -t upload
pio device monitor -b 115200
```

The configured flash layout is 2 MB and uses `partitions.csv` (one application
slot, no OTA update slot). EspBle's pinned revision ships a precompiled Classic
Bluetooth host archive; the explicit linker flags in `platformio.ini` are needed
for PlatformIO to link it. This Arduino build does not use `sdkconfig.defaults`.

Press the encoder button to compose, then type using the Classic Bluetooth
keyboard or serial monitor. Enter adds the message to the local inbox and logs
it over serial; wireless message delivery is not implemented. Escape or another
encoder press cancels. Keyboard discovery currently matches names containing
`keyboard` or `518`.

Run the host regression checks (requires Bash and g++) with:

```bash
bash tests/run_host_tests.sh
```

These check composer boundaries, message capacity, selection/read timing, and
button debounce using simulated time, pins, and display calls. Bluetooth pairing,
reconnection, and physical encoder/display behavior still require board testing.
