#pragma once
#include <Arduino.h>

enum class InputKey : uint8_t { Character, Backspace, Enter, Escape };

struct InputEvent {
  InputKey key;
  char character = 0;
};

class KeyboardInput {
 public:
  virtual ~KeyboardInput() = default;
  virtual void begin() = 0;
  virtual void update() = 0;
  virtual bool connected() const = 0;
  virtual bool pop(InputEvent& event) = 0;
};
