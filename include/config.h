#pragma once
#include <Arduino.h>
namespace PagerConfig {
constexpr uint8_t OLED_ADDRESS=0x3C;
constexpr int OLED_WIDTH=128, OLED_HEIGHT=64, OLED_SDA=21, OLED_SCL=22;
constexpr int ENCODER_CLK=34, ENCODER_DT=35, ENCODER_SW=32;
constexpr uint32_t READ_DWELL_MS=900, BUTTON_DEBOUNCE_MS=35, ENCODER_DEBOUNCE_MS=2;
}
