#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>
#include "keyboard_input.h"

class BleKeyboardHost : public KeyboardInput {
 public:
  void begin() override;
  void update() override;
  bool connected() const override { return connected_; }
  bool pop(InputEvent& event) override;
  void setTarget(const NimBLEAdvertisedDevice* device);
  void scanEnded(int reason);
  static BleKeyboardHost* instance_;
 private:
  static constexpr size_t kQueueSize = 32;
  InputEvent queue_[kQueueSize];
  volatile uint8_t head_=0, tail_=0;
  bool initialized_=false, connected_=false, scanning_=false, connecting_=false, shouldConnect_=false;
  uint32_t nextScanMs_=0;
  NimBLEAdvertisedDevice* target_=nullptr;
  NimBLEClient* client_=nullptr;
  void startScan();
  bool connectTarget();
  bool subscribeHidReports();
  void handleKeyboardReport(const uint8_t* data,size_t length);
  void push(InputEvent event);
  static void notifyCallback(NimBLERemoteCharacteristic*,uint8_t*,size_t,bool);
  static InputKey usageToKey(uint8_t usage);
  static char usageToAscii(uint8_t usage,uint8_t modifiers);
};
