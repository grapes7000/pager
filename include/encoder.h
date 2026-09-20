#pragma once
#include <Arduino.h>
class Encoder {
 public: void begin(); void update(); int consumeDelta(); bool consumeClick();
 private: int lastClk_=HIGH,delta_=0,lastButton_=HIGH,rawButton_=HIGH; bool clicked_=false; uint32_t lastEdgeMs_=0,lastButtonMs_=0;
};
