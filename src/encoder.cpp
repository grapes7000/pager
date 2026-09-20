#include "encoder.h"
#include "config.h"
void Encoder::begin(){pinMode(PagerConfig::ENCODER_CLK,INPUT);pinMode(PagerConfig::ENCODER_DT,INPUT);pinMode(PagerConfig::ENCODER_SW,INPUT_PULLUP);lastClk_=digitalRead(PagerConfig::ENCODER_CLK);lastButton_=rawButton_=digitalRead(PagerConfig::ENCODER_SW);lastButtonMs_=millis();}
void Encoder::update(){
  uint32_t now=millis();
  int clk=digitalRead(PagerConfig::ENCODER_CLK);
  if(clk!=lastClk_&&now-lastEdgeMs_>=PagerConfig::ENCODER_DEBOUNCE_MS){
    lastEdgeMs_=now;
    if(clk==LOW){int dt=digitalRead(PagerConfig::ENCODER_DT);delta_+=(dt!=clk)?1:-1;}
    lastClk_=clk;
  }
  int b=digitalRead(PagerConfig::ENCODER_SW);
  if(b!=rawButton_){rawButton_=b;lastButtonMs_=now;}
  // Accept a transition only after the input has stayed stable for the debounce interval.
  if(rawButton_!=lastButton_&&now-lastButtonMs_>=PagerConfig::BUTTON_DEBOUNCE_MS){
    lastButton_=rawButton_;
    if(lastButton_==LOW)clicked_=true;
  }
}
int Encoder::consumeDelta(){int v=delta_;delta_=0;return v;} bool Encoder::consumeClick(){bool v=clicked_;clicked_=false;return v;}
