#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "config.h"
#include "encoder.h"
#include "message_store.h"
#include "message_view.h"
#include "composer.h"

Adafruit_SH1106G display(PagerConfig::OLED_WIDTH,PagerConfig::OLED_HEIGHT,&Wire,-1);
Encoder encoder;
MessageStore messages;
MessageView view(display);
Composer composer(display);
uint32_t nextMessageId=100;

void seed(){messages.add({1,"Lakota","where are u?",0,true,true});messages.add({2,"Brooke","building the tiny pager rn",0,false,false});messages.add({3,"Lakota","this message is long enough to preview",0,true,true});messages.add({4,"Brooke","encoder scrolling eats",0,false,false});messages.add({5,"Lakota","okay text me when it works",0,true,true});}

void handleTextInput(){
  while(Serial.available()){
    char c=(char)Serial.read();
    if(c=='\r' || c=='\n'){
      String body;
      if(composer.submit(body)){
        messages.add({nextMessageId++,"Brooke",body,millis(),false,false});
        view.begin(messages);
        Serial.printf("SEND %s\n",body.c_str());
      }
    }else if(c==8 || c==127){
      composer.backspace();
    }else if(c==27){
      composer.cancel();
    }else{
      composer.append(c);
    }
  }
}

void setup(){
  Serial.begin(115200);
  Wire.begin(PagerConfig::OLED_SDA,PagerConfig::OLED_SCL);
  delay(100);
  if(!display.begin(PagerConfig::OLED_ADDRESS,true)){Serial.println("ERR OLED init failed");while(true)delay(1000);}
  encoder.begin();
  seed();
  view.begin(messages);
  view.render(messages);
  Serial.println("OK PAGER/0.2");
  Serial.println("Encoder click = compose; serial typing is temporary keyboard input.");
}

void loop(){
  encoder.update();
  if(composer.active()){
    handleTextInput();
    if(encoder.consumeClick())composer.cancel();
    composer.render();
  }else{
    int d=encoder.consumeDelta();
    if(d)view.moveSelection(d,messages);
    if(encoder.consumeClick())composer.begin();
    view.updateReadState(messages);
    if(composer.active())composer.render(); else view.render(messages);
  }
  delay(10);
}
