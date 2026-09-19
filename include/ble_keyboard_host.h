#pragma once

#include <Arduino.h>
#include <EspBleClassic.h>

#include "keyboard_input.h"

class BleKeyboardHost : public KeyboardInput {
 public:
  void begin() override;
  void update() override;
  bool connected() const override { return connected_; }
  bool pop(InputEvent& event) override;

 private:
  static constexpr size_t kQueueSize = 32;

  InputEvent queue_[kQueueSize];
  volatile uint8_t head_ = 0;
  volatile uint8_t tail_ = 0;
  volatile bool initialized_ = false;
  volatile bool connected_ = false;
  volatile bool scanning_ = false;
  volatile bool connecting_ = false;
  String targetAddress_;
  uint32_t nextScanMs_ = 0;
  EspBleClassic bluetooth_;

  void startInquiry();
  void push(InputEvent event);
  static bool looksLikeKeyboard(const String& name);
  static InputKey usageToKey(uint8_t usage, uint8_t ascii);
  static BleKeyboardHost* instance_;
};
