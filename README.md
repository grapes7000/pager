# Pager

ESP32-based wireless hardware messenger.

## Prototype hardware

- ESP32-WROOM dev board
- 128x64 SSD1306 I2C OLED
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
