#pragma once
#include <Arduino.h>
struct Message { uint32_t id; String sender; String body; uint32_t timestamp; bool incoming; bool unread; };
