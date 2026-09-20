#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include "config.h"
#include "encoder.h"
#include "message_store.h"
#include "message_view.h"
#include "composer.h"
#include "ble_keyboard_host.h"
#include "pager_link.h"

Adafruit_SH1106G display(PagerConfig::OLED_WIDTH,PagerConfig::OLED_HEIGHT,&Wire,-1);
Encoder encoder;
MessageStore messages;
MessageView view(display);
Composer composer(display);
BleKeyboardHost keyboard;
PagerLink link;
uint32_t nextMessageId=1;

void sendComposed(){
  String body;
  if(composer.submit(body)){
    uint32_t id=nextMessageId++;
    bool queued=link.queueMessage(id,body);
    messages.add({id,link.deviceName(),body,millis(),false,false});
    view.begin(messages);
    Serial.printf("SEND id=%lu queued=%s pending=%u text=%s\n",
                  (unsigned long)id,queued?"yes":"no",
                  (unsigned)link.pendingCount(),body.c_str());
  }
}

void receiveLinkedMessages(){
  PagerIncomingMessage incoming;
  while(link.popReceived(incoming)){
    messages.add({incoming.id,link.peerName(),incoming.body,millis(),true,true});
    view.begin(messages);
    Serial.printf("RECV id=%lu from=%s text=%s\n",
                  (unsigned long)incoming.id,link.peerName(),incoming.body.c_str());
  }
}

void handleKeyboardInput(){
  InputEvent event;
  while(keyboard.pop(event)){
    switch(event.key){
      case InputKey::Character: composer.append(event.character); break;
      case InputKey::Backspace: composer.backspace(); break;
      case InputKey::Enter: sendComposed(); break;
      case InputKey::Escape: composer.cancel(); break;
    }
  }
}

void handleSerialFallback(){
  while(Serial.available()){
    char c=(char)Serial.read();
    if(c=='\r'||c=='\n')sendComposed();
    else if(c==8||c==127)composer.backspace();
    else if(c==27)composer.cancel();
    else composer.append(c);
  }
}

void setup(){
  Serial.begin(115200);
  Wire.begin(PagerConfig::OLED_SDA,PagerConfig::OLED_SCL);
  delay(100);
  if(!display.begin(PagerConfig::OLED_ADDRESS,true)){Serial.println("ERR OLED init failed");while(true)delay(1000);}
  encoder.begin();
  view.begin(messages);
  view.render(messages);
  Serial.printf("OK PAGER/0.4 %s\n",link.deviceName());

  Serial.println("Starting direct pager link...");
  if(!link.begin())Serial.println("[LINK] disabled after initialization failure");

  Serial.println("Starting BLE HID keyboard discovery...");
  keyboard.begin();
}

void loop(){
  encoder.update();
  keyboard.update();
  link.update();
  receiveLinkedMessages();

  // Rotation while composing must not scroll the inbox after returning to it.
  int d=encoder.consumeDelta();

  if(composer.active()){
    handleKeyboardInput();
    handleSerialFallback();
    if(encoder.consumeClick())composer.cancel();
    if(composer.active())composer.render();
    else {
      view.resetDwell();
      view.render(messages);
    }
  }else{
    InputEvent ignored;
    while(keyboard.pop(ignored)){}
    while(Serial.available())Serial.read();
    if(d)view.moveSelection(d,messages);
    if(encoder.consumeClick())composer.begin();
    if(!composer.active())view.updateReadState(messages);
    if(composer.active())composer.render(); else view.render(messages);
  }
  delay(10);
}
