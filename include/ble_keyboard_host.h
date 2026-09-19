#pragma once

#include <Arduino.h>
#include <esp_gap_bt_api.h>
#include <esp_hidh.h>

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
  volatile bool hidReady_ = false;
  volatile bool connected_ = false;
  volatile bool scanning_ = false;
  volatile bool connecting_ = false;
  volatile bool targetFound_ = false;

  esp_bd_addr_t targetBda_ = {0};
  uint8_t previous_[6] = {0};
  uint32_t nextScanMs_ = 0;

  void startInquiry();
  void handleGapEvent(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param);
  void handleHidEvent(esp_hidh_event_t event, esp_hidh_event_data_t* param);
  void handleReport(const uint8_t* data, size_t length, uint16_t reportId);
  void push(InputEvent event);

  static bool looksLikeKeyboard(const String& name);
  static char usageToAscii(uint8_t usage, bool shift);
  static void gapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param);
  static void hidCallback(void* arg, esp_event_base_t base, int32_t eventId, void* eventData);
  static BleKeyboardHost* instance_;
};
