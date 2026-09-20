#include "message_view.h"
#include "config.h"
MessageView::MessageView(Adafruit_SH1106G& d):display_(d){}
void MessageView::begin(MessageStore& s){selected_=s.size()?s.size()-1:0;resetDwell();}
void MessageView::resetDwell(){selectedSinceMs_=millis();}
void MessageView::moveSelection(int d,MessageStore& s){if(!s.size()||!d)return;int n=(int)selected_+constrain(d,-(int)s.size(),(int)s.size());n=constrain(n,0,(int)s.size()-1);if((size_t)n!=selected_){selected_=n;resetDwell();}}
void MessageView::updateReadState(MessageStore& s){Message*m=s.at(selected_);if(m&&m->unread&&millis()-selectedSinceMs_>=PagerConfig::READ_DWELL_MS)m->unread=false;}
String MessageView::preview(const String&t,size_t n)const{if(t.length()<=n)return t;if(n<=3)return t.substring(0,n);return t.substring(0,n-3)+"...";}
void MessageView::render(const MessageStore&s){display_.clearDisplay();display_.setTextSize(1);display_.setTextColor(SH110X_WHITE);display_.setCursor(0,0);display_.print("PAGER");display_.setCursor(88,0);display_.print("NEW ");display_.print(s.unreadCount());display_.drawLine(0,9,127,9,SH110X_WHITE);if(!s.size()){display_.setCursor(20,30);display_.print("No messages");display_.display();return;}int first=(int)selected_-1;if(first<0)first=0;if(s.size()>3&&first>(int)s.size()-3)first=(int)s.size()-3;for(int row=0;row<3;++row){size_t i=first+row;const Message*m=s.at(i);if(!m)break;int y=12+row*18;bool sel=i==selected_;if(sel){display_.fillRect(0,y-1,128,18,SH110X_WHITE);display_.setTextColor(SH110X_BLACK);}else display_.setTextColor(SH110X_WHITE);display_.setCursor(2,y);display_.print(sel?">":" ");display_.print(m->unread?"*":" ");display_.print(preview(m->sender,8));display_.setCursor(14,y+9);display_.print(preview(m->body,18));}display_.setTextColor(SH110X_WHITE);display_.display();}
