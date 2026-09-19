#include "ble_keyboard_host.h"

#include <cstring>
#include <esp_bt.h>
#include <esp_bt_device.h>
#include <esp_bt_main.h>
#include <esp32-hal-bt.h>
#include <nvs_flash.h>

BleKeyboardHost* BleKeyboardHost::instance_ = nullptr;

bool BleKeyboardHost::looksLikeKeyboard(const String& name) {
  String lower = name;
  lower.toLowerCase();
  return lower.indexOf("keyboard") >= 0 || lower.indexOf("518") >= 0;
}

void BleKeyboardHost::begin() {
  instance_ = this;

  // Let Arduino's own BT HAL bring up the controller. Arduino's ESP32 core
  // owns the controller lifecycle, so calling esp_bt_controller_init() here
  // directly can return INVALID_STATE even immediately after boot.
  Serial.printf("[BT] before btStart: started=%d controller=%d\\n",\n                (int)btStarted(), (int)esp_bt_controller_get_status());\n  if (!btStarted() && !btStart()) {
    Serial.println("[BT] Arduino Bluetooth controller start failed");
    return;
  }
\n  Serial.printf("[BT] after btStart: started=%d controller=%d\\n",\n                (int)btStarted(), (int)esp_bt_controller_get_status());
  esp_err_t err;
  esp_bluedroid_status_t bluedroid = esp_bluedroid_get_status();
  if (bluedroid == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
    err = esp_bluedroid_init();
    if (err != ESP_OK) {
      Serial.printf("[BT] Bluedroid init failed: %s\n", esp_err_to_name(err));
      return;
    }
    bluedroid = esp_bluedroid_get_status();
  }

  if (bluedroid == ESP_BLUEDROID_STATUS_INITIALIZED) {
    err = esp_bluedroid_enable();
    if (err != ESP_OK) {
      Serial.printf("[BT] Bluedroid enable failed: %s\n", esp_err_to_name(err));
      return;
    }
  } else if (bluedroid != ESP_BLUEDROID_STATUS_ENABLED) {
    Serial.printf("[BT] unexpected Bluedroid state: %d\n", (int)bluedroid);
    return;
  }

  err = esp_bt_gap_register_callback(gapCallback);
  if (err != ESP_OK) {
    Serial.printf("[BT] GAP callback failed: %s\n", esp_err_to_name(err));
    return;
  }

  esp_hidh_config_t hidCfg = {};
  hidCfg.callback = hidCallback;
  hidCfg.event_stack_size = 4096;
  hidCfg.callback_arg = this;
  err = esp_hidh_init(&hidCfg);
  if (err != ESP_OK) {
    Serial.printf("[BT] HID host init failed: %s\n", esp_err_to_name(err));
    return;
  }

  initialized_ = true;
  hidReady_ = true;
  Serial.printf("[BT] Classic HID ready (controller=%d, bluedroid=%d)\n",
                (int)esp_bt_controller_get_status(),
                (int)esp_bluedroid_get_status());
  startInquiry();
}

void BleKeyboardHost::startInquiry() {
  if (!initialized_ || scanning_ || connecting_ || connected_) return;
  targetFound_ = false;
  Serial.println("[BT] scanning Bluetooth Classic devices...");
  esp_err_t err = esp_bt_gap_start_discovery(ESP_BT_INQ_MODE_GENERAL_INQUIRY, 8, 0);
  if (err == ESP_OK) {
    scanning_ = true;
  } else {
    Serial.printf("[BT] inquiry start failed: %s\n", esp_err_to_name(err));
    nextScanMs_ = millis() + 3000;
  }
}

void BleKeyboardHost::update() {
  if (!connected_ && !scanning_ && !connecting_ &&
      static_cast<int32_t>(millis() - nextScanMs_) >= 0) {
    startInquiry();
  }
}

void BleKeyboardHost::gapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param) {
  if (instance_) instance_->handleGapEvent(event, param);
}

void BleKeyboardHost::hidCallback(void* arg, esp_event_base_t, int32_t eventId, void* eventData) {
  auto* self = static_cast<BleKeyboardHost*>(arg);
  if (self) self->handleHidEvent(static_cast<esp_hidh_event_t>(eventId),
                                 static_cast<esp_hidh_event_data_t*>(eventData));
}

void BleKeyboardHost::handleGapEvent(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param) {
  if (event == ESP_BT_GAP_DISC_RES_EVT) {
    String name;
    int8_t rssi = 0;
    bool haveRssi = false;

    for (int i = 0; i < param->disc_res.num_prop; ++i) {
      esp_bt_gap_dev_prop_t& p = param->disc_res.prop[i];
      if (p.type == ESP_BT_GAP_DEV_PROP_BDNAME && p.val && p.len) {
        name = String(static_cast<const char*>(p.val), p.len);
      } else if (p.type == ESP_BT_GAP_DEV_PROP_EIR && p.val) {
        uint8_t len = 0;
        uint8_t* found = esp_bt_gap_resolve_eir_data(
            static_cast<uint8_t*>(p.val), ESP_BT_EIR_TYPE_CMPL_LOCAL_NAME, &len);
        if (!found) {
          found = esp_bt_gap_resolve_eir_data(
              static_cast<uint8_t*>(p.val), ESP_BT_EIR_TYPE_SHORT_LOCAL_NAME, &len);
        }
        if (found && len) name = String(reinterpret_cast<const char*>(found), len);
      } else if (p.type == ESP_BT_GAP_DEV_PROP_RSSI && p.val) {
        rssi = *static_cast<int8_t*>(p.val);
        haveRssi = true;
      }
    }

    char addr[18];
    snprintf(addr, sizeof(addr), "%02x:%02x:%02x:%02x:%02x:%02x",
             param->disc_res.bda[0], param->disc_res.bda[1], param->disc_res.bda[2],
             param->disc_res.bda[3], param->disc_res.bda[4], param->disc_res.bda[5]);
    Serial.printf("[BT] Classic seen: %s", addr);
    if (name.length()) Serial.printf(" name=%s", name.c_str());
    if (haveRssi) Serial.printf(" rssi=%d", rssi);
    Serial.println();

    if (!targetFound_ && name.length() && looksLikeKeyboard(name)) {
      memcpy(targetBda_, param->disc_res.bda, ESP_BD_ADDR_LEN);
      targetFound_ = true;
      Serial.printf("[BT] keyboard matched: %s name=%s\n", addr, name.c_str());
      esp_bt_gap_cancel_discovery();
    }
    return;
  }

  if (event == ESP_BT_GAP_DISC_STATE_CHANGED_EVT &&
      param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
    scanning_ = false;
    if (targetFound_ && !connected_ && !connecting_) {
      connecting_ = true;
      Serial.println("[BT] opening Classic HID keyboard...");
      esp_hidh_dev_t* dev = esp_hidh_dev_open(targetBda_, ESP_HID_TRANSPORT_BT, 0);
      if (!dev) {
        Serial.println("[BT] HID open start failed");
        connecting_ = false;
        targetFound_ = false;
        nextScanMs_ = millis() + 2500;
      }
    } else if (!connected_) {
      Serial.println("[BT] no matching keyboard found; will rescan");
      nextScanMs_ = millis() + 2000;
    }
  }
}

void BleKeyboardHost::handleHidEvent(esp_hidh_event_t event, esp_hidh_event_data_t* param) {
  switch (event) {
    case ESP_HIDH_OPEN_EVENT:
      connecting_ = false;
      if (param->open.status == ESP_OK) {
        connected_ = true;
        Serial.printf("[BT] keyboard connected: %s\n",
                      esp_hidh_dev_name_get(param->open.dev));
      } else {
        connected_ = false;
        targetFound_ = false;
        Serial.printf("[BT] keyboard open failed: %s\n", esp_err_to_name(param->open.status));
        nextScanMs_ = millis() + 2500;
      }
      break;

    case ESP_HIDH_INPUT_EVENT:
      handleReport(param->input.data, param->input.length, param->input.report_id);
      break;

    case ESP_HIDH_CLOSE_EVENT:
      connected_ = false;
      connecting_ = false;
      targetFound_ = false;
      memset(previous_, 0, sizeof(previous_));
      Serial.println("[BT] keyboard disconnected");
      nextScanMs_ = millis() + 2000;
      break;

    default:
      break;
  }
}

void BleKeyboardHost::handleReport(const uint8_t* data, size_t length, uint16_t) {
  // Standard keyboard report: modifiers, reserved, six key usages.
  if (!data || length < 8) return;
  const bool shift = (data[0] & 0x22) != 0;

  for (size_t i = 2; i < 8; ++i) {
    const uint8_t usage = data[i];
    if (!usage) continue;

    bool wasDown = false;
    for (uint8_t old : previous_) {
      if (old == usage) {
        wasDown = true;
        break;
      }
    }
    if (wasDown) continue;

    if (usage == 0x28) push(InputEvent(InputKey::Enter));
    else if (usage == 0x29) push(InputEvent(InputKey::Escape));
    else if (usage == 0x2A) push(InputEvent(InputKey::Backspace));
    else {
      char c = usageToAscii(usage, shift);
      if (c) push(InputEvent(InputKey::Character, c));
    }
  }
  memcpy(previous_, data + 2, 6);
}

char BleKeyboardHost::usageToAscii(uint8_t u, bool shift) {
  if (u >= 0x04 && u <= 0x1D) {
    char c = 'a' + (u - 0x04);
    return shift ? static_cast<char>(c - 'a' + 'A') : c;
  }
  static const char normal[] = "1234567890";
  static const char shifted[] = "!@#$%^&*()";
  if (u >= 0x1E && u <= 0x27) return shift ? shifted[u - 0x1E] : normal[u - 0x1E];
  switch (u) {
    case 0x2C: return ' ';
    case 0x2D: return shift ? '_' : '-';
    case 0x2E: return shift ? '+' : '=';
    case 0x2F: return shift ? '{' : '[';
    case 0x30: return shift ? '}' : ']';
    case 0x31: return shift ? '|' : '\\';
    case 0x33: return shift ? ':' : ';';
    case 0x34: return shift ? '"' : '\'';
    case 0x35: return shift ? '~' : '`';
    case 0x36: return shift ? '<' : ',';
    case 0x37: return shift ? '>' : '.';
    case 0x38: return shift ? '?' : '/';
    default: return 0;
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
