#pragma once
#include <Adafruit_SSD1306.h>
#include "message_store.h"
class MessageView {
 public: explicit MessageView(Adafruit_SSD1306& d); void begin(MessageStore&); void moveSelection(int,MessageStore&); void updateReadState(MessageStore&); void render(const MessageStore&); size_t selectedIndex()const{return selected_;}
 private: Adafruit_SSD1306& display_; size_t selected_=0; uint32_t selectedSinceMs_=0; String preview(const String&,size_t)const; void resetDwell();
};
