#include "ble_keyboard_host.h"

BleKeyboardHost* BleKeyboardHost::instance_ = nullptr;

bool BleKeyboardHost::looksLikeKeyboard(const String& name){
  String lower=name;
  lower.toLowerCase();
  return lower.indexOf("keyboard")>=0 || lower.indexOf("518")>=0;
}

void BleKeyboardHost::begin(){
  instance_=this;

  auto& hid=bluetooth_.hidHost();
  hid.setKeyboardLayout(EspBleKeyboardLayout::EnUs);

  hid.onKeyboard([](const EspBleClassicHidKeyboardEvent& event){
    if(instance_)instance_->handleKey(event);
  });

  hid.onConnected([](const EspBleClassicHidConnection& connection){
    if(!instance_)return;
    instance_->connected_=true;
    instance_->targetAddress_=connection.peerAddress;
    Serial.printf("[BT] Classic HID keyboard connected: %s\n",
                  connection.peerAddress.c_str());
  });

  hid.onConnectionFailed([](const EspBleClassicHidConnectionFailure& failure){
    if(!instance_)return;
    instance_->connected_=false;
    instance_->targetFound_=false;
    instance_->nextInquiryMs_=millis()+2000;
    Serial.printf("[BT] Classic HID connection failed: %s (%s)\n",
                  failure.peerAddress.c_str(),failure.detail.c_str());
  });

  EspBleClassicConfig config;
  config.deviceName="PAGER";
  if(!bluetooth_.begin(config)){
    Serial.printf("[BT] Classic init failed: %s: %s\n",
                  bluetooth_.lastErrorName(),
                  bluetooth_.lastErrorDetail().c_str());
    return;
  }
  if(!hid.begin()){
    Serial.printf("[BT] Classic HID host init failed: %s: %s\n",
                  bluetooth_.lastErrorName(),
                  bluetooth_.lastErrorDetail().c_str());
    return;
  }

  bluetooth_.inquiry().onResult([](const EspBleClassicInquiryResult& result){
    if(!instance_)return;
    Serial.printf("[BT] Classic seen: %s",result.address.c_str());
    if(!result.name.isEmpty())Serial.printf(" name=%s",result.name.c_str());
    if(result.hasClassOfDevice)
      Serial.printf(" cod=0x%06x",static_cast<unsigned>(result.classOfDevice));
    if(result.hasRssi)Serial.printf(" rssi=%d",result.rssi);
    Serial.println();

    if(!instance_->targetFound_ && !result.name.isEmpty() &&
       looksLikeKeyboard(result.name)){
      instance_->targetFound_=true;
      instance_->targetAddress_=result.address;
      Serial.printf("[BT] keyboard matched: %s name=%s\n",
                    result.address.c_str(),result.name.c_str());
      instance_->bluetooth_.inquiry().cancel();
    }
  });

  bluetooth_.inquiry().onComplete([](const EspBleClassicInquiryComplete& event){
    if(!instance_)return;
    instance_->inquiryRunning_=false;
    if(instance_->targetFound_ && !instance_->connected_){
      Serial.printf("[BT] connecting Classic HID: %s\n",
                    instance_->targetAddress_.c_str());
      instance_->bluetooth_.hidHost().connect(instance_->targetAddress_.c_str());
    }else if(!instance_->connected_){
      Serial.println("[BT] no matching Classic keyboard found; will rescan");
      instance_->nextInquiryMs_=millis()+2000;
    }
  });

  startInquiry();
}

void BleKeyboardHost::startInquiry(){
  if(inquiryRunning_ || connected_)return;
  targetFound_=false;
  Serial.println("[BT] scanning Bluetooth Classic devices...");
  EspBleClassicInquiryConfig config;
  config.durationSeconds=8;
  if(bluetooth_.inquiry().start(config)){
    inquiryRunning_=true;
  }else{
    Serial.printf("[BT] inquiry start failed: %s: %s\n",
                  bluetooth_.lastErrorName(),
                  bluetooth_.lastErrorDetail().c_str());
    nextInquiryMs_=millis()+3000;
  }
}

void BleKeyboardHost::update(){
  bluetooth_.update();
  if(!connected_ && !inquiryRunning_ && !targetFound_ &&
     static_cast<int32_t>(millis()-nextInquiryMs_)>=0){
    startInquiry();
  }
}

void BleKeyboardHost::handleKey(const EspBleClassicHidKeyboardEvent& event){
  if(!event.pressed)return;
  switch(event.usage){
    case 0x28: push(InputEvent(InputKey::Enter)); return;
    case 0x29: push(InputEvent(InputKey::Escape)); return;
    case 0x2A: push(InputEvent(InputKey::Backspace)); return;
    default: break;
  }
  if(event.ascii>=32 && event.ascii<=126)
    push(InputEvent(InputKey::Character,static_cast<char>(event.ascii)));
}

void BleKeyboardHost::push(InputEvent event){
  uint8_t next=(head_+1)%kQueueSize;
  if(next==tail_)return;
  queue_[head_]=event;
  head_=next;
}

bool BleKeyboardHost::pop(InputEvent& event){
  if(tail_==head_)return false;
  event=queue_[tail_];
  tail_=(tail_+1)%kQueueSize;
  return true;
}
