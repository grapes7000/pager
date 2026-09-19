#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "config.h"
#include "encoder.h"
#include "message_store.h"
#include "message_view.h"
Adafruit_SSD1306 display(PagerConfig::OLED_WIDTH,PagerConfig::OLED_HEIGHT,&Wire,-1);
Encoder encoder; MessageStore messages; MessageView view(display);
void seed(){messages.add({1,"Lakota","where are u?",0,true,true});messages.add({2,"Brooke","building the tiny pager rn",0,false,false});messages.add({3,"Lakota","this message is long enough to preview",0,true,true});messages.add({4,"Brooke","encoder scrolling eats",0,false,false});messages.add({5,"Lakota","okay text me when it works",0,true,true});}
void setup(){Serial.begin(115200);Wire.begin(PagerConfig::OLED_SDA,PagerConfig::OLED_SCL);if(!display.begin(SSD1306_SWITCHCAPVCC,PagerConfig::OLED_ADDRESS)){Serial.println("ERR OLED init failed");while(true)delay(1000);}encoder.begin();seed();view.begin(messages);view.render(messages);Serial.println("OK PAGER/0.1");}
void loop(){encoder.update();int d=encoder.consumeDelta();if(d)view.moveSelection(d,messages);if(encoder.consumeClick())Serial.printf("CLICK message=%u\n",(unsigned)view.selectedIndex());view.updateReadState(messages);view.render(messages);delay(10);}
