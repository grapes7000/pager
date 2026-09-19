#include "message_store.h"
bool MessageStore::add(const Message& m){if(count_>=kCapacity){for(size_t i=1;i<count_;++i)messages_[i-1]=messages_[i];--count_;}messages_[count_++]=m;return true;}
size_t MessageStore::size()const{return count_;}
Message* MessageStore::at(size_t i){return i<count_?&messages_[i]:nullptr;}
const Message* MessageStore::at(size_t i)const{return i<count_?&messages_[i]:nullptr;}
size_t MessageStore::unreadCount()const{size_t n=0;for(size_t i=0;i<count_;++i)if(messages_[i].unread)++n;return n;}
