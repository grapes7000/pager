#include "ble_keyboard_host.h"

static BLEUUID HID_SERVICE((uint16_t)0x1812);
static BLEUUID BOOT_KEYBOARD_INPUT((uint16_t)0x2A22);
BleKeyboardHost* BleKeyboardHost::instance_ = nullptr;

class PagerAdvertisedCallbacks : public BLEAdvertisedDeviceCallbacks {
 public:
  explicit PagerAdvertisedCallbacks(BleKeyboardHost* host):host_(host){}
  void onResult(BLEAdvertisedDevice device) override {
    if(device.haveServiceUUID() && device.isAdvertisingService(HID_SERVICE)){
      Serial.printf("[BT] HID found: %s name=%s\\n", device.getAddress().toString().c_str(), device.haveName()?device.getName().c_str():"(unknown)");
      BLEDevice::getScan()->stop();
      host_->setTarget(device.getAddress());
    }
  }
 private:
  BleKeyboardHost* host_;
};

class PagerClientCallbacks : public BLEClientCallbacks {
 public:
  explicit PagerClientCallbacks(BleKeyboardHost* host):host_(host){}
  void onConnect(BLEClient*) override {}
  void onDisconnect(BLEClient*) override { host_->markDisconnected(); }
 private:
  BleKeyboardHost* host_;
};

void BleKeyboardHost::begin(){
  instance_=this;
  BLEDevice::init("PAGER");
  scan();
}

void BleKeyboardHost::scan(){
  if(scanning_ || connected_ || connectPending_)return;
  scanning_=true;
  BLEScan* scan=BLEDevice::getScan();
  scan->setAdvertisedDeviceCallbacks(new PagerAdvertisedCallbacks(this),true);
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(80);
  scan->start(4,false);
  scanning_=false;
}

void BleKeyboardHost::setTarget(const BLEAddress& address){
  if(target_)delete target_;
  target_=new BLEAddress(address);
  connectPending_=true;
}

void BleKeyboardHost::markDisconnected(){
  connected_=false;
  memset(previous_,0,sizeof(previous_));
}

bool BleKeyboardHost::connectTarget(){
  if(!target_){Serial.println("[BT] FAIL no target");return false;}
  Serial.printf("[BT] connecting %s\\n",target_->toString().c_str());
  if(!client_){
    client_=BLEDevice::createClient();
    client_->setClientCallbacks(new PagerClientCallbacks(this));
  }
  if(!client_->connect(*target_)){Serial.println("[BT] FAIL connect");return false;}
  Serial.println("[BT] link connected");
  BLERemoteService* hid=client_->getService(HID_SERVICE);
  if(!hid){Serial.println("[BT] FAIL HID service");client_->disconnect();return false;}
  Serial.println("[BT] HID service found");
  BLERemoteCharacteristic* input=hid->getCharacteristic(BOOT_KEYBOARD_INPUT);
  if(!input){Serial.println("[BT] FAIL boot input 0x2A22 missing");client_->disconnect();return false;}
  Serial.printf("[BT] boot input found notify=%s\\n",input->canNotify()?"yes":"no");
  if(!input->canNotify()){Serial.println("[BT] FAIL boot input cannot notify");client_->disconnect();return false;}
  input->registerForNotify(notify);
  Serial.println("[BT] notifications registered");
  connected_=true;
  memset(previous_,0,sizeof(previous_));
  Serial.printf("BT keyboard connected: %s\n",target_->toString().c_str());
  return true;
}

void BleKeyboardHost::update(){
  if(connectPending_){
    connectPending_=false;
    if(!connectTarget()){
      Serial.println("BT keyboard connection failed; rescanning");
      if(target_){delete target_;target_=nullptr;}
    }
  }
  static uint32_t lastScan=0;
  if(!connected_ && !connectPending_ && millis()-lastScan>5000){
    lastScan=millis();
    scan();
  }
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

static bool held(const uint8_t* keys,uint8_t key){
  for(int i=0;i<6;++i)if(keys[i]==key)return true;
  return false;
}

void BleKeyboardHost::handleReport(const uint8_t* data,size_t length){
  if(length<8)return;
  const uint8_t modifiers=data[0];
  const bool shift=(modifiers&0x22)!=0;
  const uint8_t* keys=data+2;
  for(int i=0;i<6;++i){
    uint8_t usage=keys[i];
    if(!usage || held(previous_,usage))continue;
    if(usage==0x28)push({InputKey::Enter,0});
    else if(usage==0x29)push({InputKey::Escape,0});
    else if(usage==0x2A)push({InputKey::Backspace,0});
    else {
      char c=usageToAscii(usage,shift);
      if(c)push({InputKey::Character,c});
    }
  }
  memcpy(previous_,keys,6);
}

void BleKeyboardHost::notify(BLERemoteCharacteristic*,uint8_t* data,size_t length,bool){
  if(instance_)instance_->handleReport(data,length);
}

char BleKeyboardHost::usageToAscii(uint8_t u,bool shift){
  if(u>=0x04&&u<=0x1D)return (shift?'A':'a')+(u-0x04);
  if(u>=0x1E&&u<=0x27){
    static const char normal[]="1234567890";
    static const char shifted[]="!@#$%^&*()";
    return (shift?shifted:normal)[u-0x1E];
  }
  switch(u){
    case 0x2C:return ' ';
    case 0x2D:return shift?'_':'-';
    case 0x2E:return shift?'+':'=';
    case 0x2F:return shift?'{':'[';
    case 0x30:return shift?'}':']';
    case 0x31:return shift?'|':'\\';
    case 0x33:return shift?':':';';
    case 0x34:return shift?'"':'\'';
    case 0x35:return shift?'~':'\x60';
    case 0x36:return shift?'<':',';
    case 0x37:return shift?'>':'.';
    case 0x38:return shift?'?':'/';
    default:return 0;
  }
}
