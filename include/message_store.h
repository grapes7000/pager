#pragma once
#include <Arduino.h>
#include "message.h"
class MessageStore {
 public:
  static constexpr size_t kCapacity=16;
  bool add(const Message& message); size_t size() const;
  Message* at(size_t index); const Message* at(size_t index) const; size_t unreadCount() const;
 private: Message messages_[kCapacity]; size_t count_=0;
};
