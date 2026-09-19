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
  bool connected_ = false;
  bool inquiryRunning_ = false;
  bool targetFound_ = false;
  String targetAddress_;
  uint32_t nextInquiryMs_ = 0;

  void startInquiry();
  void push(InputEvent event);
  void handleKey(const EspBleClassicHidKeyboardEvent& event);
  static bool looksLikeKeyboard(const String& name);
  static BleKeyboardHost* instance_;
  EspBleClassic bluetooth_;
};
