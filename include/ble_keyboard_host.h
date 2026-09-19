#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include "keyboard_input.h"

class BleKeyboardHost : public KeyboardInput {
 public:
  void begin() override;
  void update() override;
  bool connected() const override { return connected_; }
  bool pop(InputEvent& event) override;
  void setTarget(const BLEAddress& address);
  void markDisconnected();

 private:
  static constexpr size_t kQueueSize = 32;
  InputEvent queue_[kQueueSize];
  volatile uint8_t head_ = 0;
  volatile uint8_t tail_ = 0;
  bool connected_ = false;
  bool scanning_ = false;
  bool connectPending_ = false;
  BLEAddress* target_ = nullptr;
  BLEClient* client_ = nullptr;
  uint8_t previous_[6] = {0};

  void scan();
  bool connectTarget();
  void push(InputEvent event);
  void handleReport(const uint8_t* data, size_t length);
  static char usageToAscii(uint8_t usage, bool shift);
  static void notify(BLERemoteCharacteristic*, uint8_t*, size_t, bool);
  static BleKeyboardHost* instance_;
};
