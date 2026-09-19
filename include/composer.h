#pragma once
#include <Arduino.h>
#include <Adafruit_SH110X.h>

class Composer {
 public:
  static constexpr size_t kMaxLength = 160;
  explicit Composer(Adafruit_SH1106G& display);
  void begin();
  void cancel();
  bool active() const { return active_; }
  void append(char c);
  void backspace();
  bool submit(String& text);
  void render();
 private:
  Adafruit_SH1106G& display_;
  String text_;
  bool active_ = false;
  void drawWrappedText();
};
