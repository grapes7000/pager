#include "ble_keyboard_host.h"

BleKeyboardHost* BleKeyboardHost::instance_ = nullptr;

bool BleKeyboardHost::looksLikeKeyboard(const String& name) {
  String lower = name;
  lower.toLowerCase();
  return lower.indexOf("keyboard") >= 0 || lower.indexOf("518") >= 0;
}

InputKey BleKeyboardHost::usageToKey(uint8_t usage, uint8_t ascii) {
  if (usage == 0x28) return InputKey::Enter;
  if (usage == 0x29) return InputKey::Escape;
  if (usage == 0x2A) return InputKey::Backspace;
  return InputKey::Character;
}

void BleKeyboardHost::begin() {
  instance_ = this;
  Serial.println("[BT] starting EspBle Bluetooth Classic HID host...");

  auto& hid = bluetooth_.hidHost();
  hid.setKeyboardLayout(EspBleKeyboardLayout::EnUs);

  hid.onKeyboard([](const EspBleClassicHidKeyboardEvent& event) {
    if (!instance_ || !event.pressed) return;

    Serial.printf("[BT] key usage=0x%02x ascii=0x%02x\n",
                  event.usage, event.ascii);

    InputKey key = usageToKey(event.usage, event.ascii);
    if (key == InputKey::Character) {
      if (event.ascii) instance_->push(InputEvent(key, static_cast<char>(event.ascii)));
    } else {
      instance_->push(InputEvent(key));
    }
  });

  hid.onInputReport([](const EspBleClassicHidReport& report) {
    Serial.printf("[BT] raw HID report id=%u len=%u\n",
                  report.reportId,
                  static_cast<unsigned>(report.value.length()));
  });

  hid.onConnected([](const EspBleClassicHidConnection& connection) {
    if (!instance_) return;
    instance_->connected_ = true;
    instance_->connecting_ = false;
    instance_->scanning_ = false;
    Serial.printf("[BT] keyboard connected: %s\n", connection.peerAddress.c_str());
  });

  hid.onConnectionFailed([](const EspBleClassicHidConnectionFailure& failure) {
    if (!instance_) return;
    instance_->connected_ = false;
    instance_->connecting_ = false;
    instance_->targetAddress_ = "";
    instance_->nextScanMs_ = millis() + 2500;
    Serial.printf("[BT] keyboard connection failed: %s\n", failure.detail.c_str());
  });

  hid.onDisconnected([](const EspBleClassicHidConnection& connection) {
    if (!instance_) return;
    instance_->connected_ = false;
    instance_->connecting_ = false;
    instance_->targetAddress_ = "";
    instance_->nextScanMs_ = millis() + 2500;
    Serial.printf("[BT] keyboard disconnected: %s\n", connection.peerAddress.c_str());
  });

  bluetooth_.inquiry().onResult([](const EspBleClassicInquiryResult& result) {
    if (!instance_) return;

    Serial.printf("[BT] Classic seen: %s", result.address.c_str());
    if (!result.name.isEmpty()) Serial.printf(" name=%s", result.name.c_str());
    if (result.hasRssi) Serial.printf(" rssi=%d", result.rssi);
    Serial.println();

    String name(result.name.c_str());
    if (instance_->targetAddress_.isEmpty() && looksLikeKeyboard(name)) {
      instance_->targetAddress_ = result.address.c_str();
      Serial.printf("[BT] keyboard matched: %s name=%s\n",
                    result.address.c_str(), result.name.c_str());
      instance_->bluetooth_.inquiry().stop();
    }
  });

  bluetooth_.inquiry().onComplete([](const EspBleClassicInquiryComplete&) {
    if (!instance_) return;
    instance_->scanning_ = false;

    if (!instance_->targetAddress_.isEmpty() &&
        !instance_->connected_ && !instance_->connecting_) {
      instance_->connecting_ = true;
      Serial.printf("[BT] connecting HID host to %s...\n",
                    instance_->targetAddress_.c_str());
      if (!instance_->bluetooth_.hidHost().connect(instance_->targetAddress_.c_str())) {
        Serial.printf("[BT] connect request rejected: %s\n",
                      instance_->bluetooth_.lastErrorDetail().c_str());
        instance_->connecting_ = false;
        instance_->targetAddress_ = "";
        instance_->nextScanMs_ = millis() + 2500;
      }
    } else if (!instance_->connected_) {
      Serial.println("[BT] no matching keyboard found; will rescan");
      instance_->nextScanMs_ = millis() + 2000;
    }
  });

  EspBleClassicConfig config;
  config.deviceName = "Pager Keyboard Host";
  if (!bluetooth_.begin(config)) {
    Serial.printf("[BT] Classic init failed: %s: %s\n",
                  bluetooth_.lastErrorName(),
                  bluetooth_.lastErrorDetail().c_str());
    return;
  }

  if (!hid.begin()) {
    Serial.printf("[BT] HID host init failed: %s: %s\n",
                  bluetooth_.lastErrorName(),
                  bluetooth_.lastErrorDetail().c_str());
    return;
  }

  initialized_ = true;
  Serial.println("[BT] Classic HID host ready");
  startInquiry();
}

void BleKeyboardHost::startInquiry() {
  if (!initialized_ || scanning_ || connecting_ || connected_) return;

  targetAddress_ = "";
  EspBleClassicInquiryConfig config;
  config.durationSeconds = 8;

  Serial.println("[BT] scanning Bluetooth Classic devices...");
  if (bluetooth_.inquiry().start(config)) {
    scanning_ = true;
  } else {
    Serial.printf("[BT] inquiry start failed: %s\n",
                  bluetooth_.lastErrorDetail().c_str());
    nextScanMs_ = millis() + 3000;
  }
}

void BleKeyboardHost::update() {
  bluetooth_.update();

  if (initialized_ && !connected_ && !scanning_ && !connecting_ &&
      static_cast<int32_t>(millis() - nextScanMs_) >= 0) {
    startInquiry();
  }
}

void BleKeyboardHost::push(InputEvent event) {
  uint8_t next = (head_ + 1) % kQueueSize;
  if (next == tail_) return;
  queue_[head_] = event;
  head_ = next;
}

bool BleKeyboardHost::pop(InputEvent& event) {
  if (tail_ == head_) return false;
  event = queue_[tail_];
  tail_ = (tail_ + 1) % kQueueSize;
  return true;
}
