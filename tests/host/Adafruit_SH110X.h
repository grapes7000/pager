#pragma once
#include "Arduino.h"
constexpr int SH110X_WHITE=1, SH110X_BLACK=0;
class Adafruit_SH1106G {
 public:
  int cursorX=-1, cursorY=-1;
  void clearDisplay() {}
  void setTextSize(int) {}
  void setTextColor(int) {}
  void setCursor(int, int) {}
  template<class T> void print(const T&) {}
  void drawFastHLine(int x, int y, int, int) { cursorX=x; cursorY=y; }
  void drawLine(int, int, int, int, int) {}
  void fillRect(int, int, int, int, int) {}
  void display() {}
};
