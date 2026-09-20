#include "ble_keyboard_host.h"
namespace {
#ifndef PAGER_KEYBOARD_ADDRESS
#define PAGER_KEYBOARD_ADDRESS "e8:74:76:2d:cf:db"
#endif
constexpr char kKeyboardAddress[]=PAGER_KEYBOARD_ADDRESS;
const NimBLEUUID kHidService((uint16_t)0x1812);

bool addressMatches(const std::string& seen, const char* configured) {
 if(seen.size()!=strlen(configured))return false;
 for(size_t i=0;i<seen.size();++i){
  char a=seen[i],b=configured[i];
  if(a>='A'&&a<='Z')a+='a'-'A';
  if(b>='A'&&b<='Z')b+='a'-'A';
  if(a!=b)return false;
 }
 return true;
}
class ScanCallbacks:public NimBLEScanCallbacks{
 public:
 void onResult(const NimBLEAdvertisedDevice* d) override{
  auto* h=BleKeyboardHost::instance_; if(!h)return;
  Serial.printf("[BLE] seen: %s",d->getAddress().toString().c_str());
  if(d->haveName())Serial.printf(" name=%s",d->getName().c_str()); Serial.println();
  bool addr=addressMatches(d->getAddress().toString(),kKeyboardAddress);
  bool hid=d->isAdvertisingService(kHidService);
  if(addr){h->setTarget(d);Serial.printf("[BLE] keyboard found: %s%s\n",d->getAddress().toString().c_str(),hid?" HID=1812":"");NimBLEDevice::getScan()->stop();}
 }
 void onScanEnd(const NimBLEScanResults&,int reason) override{if(BleKeyboardHost::instance_)BleKeyboardHost::instance_->scanEnded(reason);}
}; ScanCallbacks scanCallbacks;
}
BleKeyboardHost* BleKeyboardHost::instance_=nullptr;
void BleKeyboardHost::setTarget(const NimBLEAdvertisedDevice* d){delete target_;target_=new NimBLEAdvertisedDevice(*d);shouldConnect_=true;}
void BleKeyboardHost::scanEnded(int reason){scanning_=false;if(!shouldConnect_){Serial.printf("[BLE] scan ended (%d); keyboard not found\n",reason);nextScanMs_=millis()+1500;}}
InputKey BleKeyboardHost::usageToKey(uint8_t u){if(u==0x28)return InputKey::Enter;if(u==0x29)return InputKey::Escape;if(u==0x2A)return InputKey::Backspace;return InputKey::Character;}
char BleKeyboardHost::usageToAscii(uint8_t u,uint8_t m){
 bool s=m&0x22;
 if(u>=0x04&&u<=0x1d){char c='a'+u-0x04;return s?c-32:c;}
 if(u>=0x1e&&u<=0x27){static const char a[]="1234567890",b[]="!@#$%^&*()";return s?b[u-0x1e]:a[u-0x1e];}
 switch(u){case 0x2c:return ' ';case 0x2d:return s?'_':'-';case 0x2e:return s?'+':'=';case 0x2f:return s?'{':'[';case 0x30:return s?'}':']';case 0x31:return s?'|':'\\';case 0x33:return s?':':';';case 0x34:return s?'"':39;case 0x35:return s?'~':96;case 0x36:return s?'<':',';case 0x37:return s?'>':'.';case 0x38:return s?'?':'/';default:return 0;}
}
void BleKeyboardHost::begin(){
 instance_=this;Serial.printf("[BLE] starting BLE HID keyboard client; target=%s\n",kKeyboardAddress);
 NimBLEDevice::init("Pager Keyboard Host");
 NimBLEDevice::setSecurityAuth(true,true,true);NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
 auto* s=NimBLEDevice::getScan();s->setScanCallbacks(&scanCallbacks,false);s->setActiveScan(true);s->setInterval(45);s->setWindow(30);
 initialized_=true;startScan();
}
void BleKeyboardHost::startScan(){
 if(!initialized_||scanning_||connecting_||connected_)return;
 Serial.printf("[BLE] scanning for configured keyboard %s...\n",kKeyboardAddress);scanning_=true;shouldConnect_=false;
 if(!NimBLEDevice::getScan()->start(8000,false,true)){scanning_=false;Serial.println("[BLE] scan failed to start");nextScanMs_=millis()+2000;}
}
bool BleKeyboardHost::connectTarget(){
 if(!target_)return false;connecting_=true;shouldConnect_=false;if(!client_)client_=NimBLEDevice::createClient();
 Serial.printf("[BLE] connecting to %s...\n",target_->getAddress().toString().c_str());
 if(!client_->connect(target_)){Serial.println("[BLE] connection failed");connecting_=false;nextScanMs_=millis()+2000;return false;}
 connected_=true;connecting_=false;Serial.println("[BLE] connected; discovering services...");
 auto services=client_->getServices(true);
 Serial.printf("[BLE] discovered %u service(s):\n",(unsigned)services.size());
 for(const auto& entry:services){
  auto* service=entry.second;
  Serial.printf("[BLE] service %s\n",service->getUUID().toString().c_str());
 }
 Serial.println("[BLE] checking HID service 1812...");
 subscribeHidReports();return true;
}
void BleKeyboardHost::subscribeHidReports(){
 auto* hid=client_->getService(kHidService,false);if(!hid){Serial.println("[BLE] ERROR: HID service 1812 not found in discovered services");return;}
 size_t n=0;for(auto* c:hid->getCharacteristics(true)){if((c->canNotify()||c->canIndicate())&&c->subscribe(c->canNotify(),notifyCallback,true)){++n;Serial.printf("[BLE] subscribed %s\n",c->getUUID().toString().c_str());}}
 Serial.printf("[BLE] HID subscriptions: %u\n",(unsigned)n);
}
void BleKeyboardHost::notifyCallback(NimBLERemoteCharacteristic*,uint8_t* d,size_t n,bool){if(instance_)instance_->handleKeyboardReport(d,n);}
void BleKeyboardHost::handleKeyboardReport(const uint8_t* d,size_t n){
 Serial.printf("[BLE] HID report len=%u:",(unsigned)n);for(size_t i=0;i<n;++i)Serial.printf(" %02x",d[i]);Serial.println();
 if(n<8)return;uint8_t m=d[0];for(size_t i=2;i<8;++i){uint8_t u=d[i];if(!u)continue;InputKey k=usageToKey(u);if(k==InputKey::Character){char a=usageToAscii(u,m);if(a)push(InputEvent(k,a));}else push(InputEvent(k));}
}
void BleKeyboardHost::update(){
 if(!initialized_)return;if(connected_&&client_&&!client_->isConnected()){connected_=false;Serial.println("[BLE] keyboard disconnected");nextScanMs_=millis()+1000;}
 if(shouldConnect_&&!connecting_&&!connected_)connectTarget();
 if(!connected_&&!connecting_&&!scanning_&&(int32_t)(millis()-nextScanMs_)>=0)startScan();
}
void BleKeyboardHost::push(InputEvent e){uint8_t n=(head_+1)%kQueueSize;if(n==tail_)return;queue_[head_]=e;head_=n;}
bool BleKeyboardHost::pop(InputEvent& e){if(tail_==head_)return false;e=queue_[tail_];tail_=(tail_+1)%kQueueSize;return true;}
