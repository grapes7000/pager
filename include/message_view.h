#pragma once
#include <Adafruit_SH110X.h>
#include "message_store.h"
class MessageView {
 public: explicit MessageView(Adafruit_SH1106G& d); void begin(MessageStore&); void moveSelection(int,MessageStore&); void updateReadState(MessageStore&); void render(const MessageStore&); size_t selectedIndex()const{return selected_;}
  void resetDwell();
 private: Adafruit_SH1106G& display_; size_t selected_=0; uint32_t selectedSinceMs_=0; String preview(const String&,size_t)const;
};
