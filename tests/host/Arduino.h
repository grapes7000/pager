#pragma once
#include <algorithm>
#include <cstdint>
#include <string>

using std::size_t;
using std::min;
using std::max;
template <class T> T constrain(T v, T lo, T hi) { return std::clamp(v, lo, hi); }
constexpr int HIGH=1, LOW=0, INPUT=0, INPUT_PULLUP=2;
inline uint32_t fakeMillis=0;
inline int fakePins[40]={};
inline uint32_t millis() { return fakeMillis; }
inline void pinMode(int, int) {}
inline int digitalRead(int pin) { return fakePins[pin]; }

class String : public std::string {
 public:
  using std::string::string;
  String(const std::string& s) : std::string(s) {}
  String substring(size_t from, size_t to=std::string::npos) const {
    return substr(from, to==std::string::npos ? to : to-from);
  }
  void remove(size_t from) { erase(from); }
};
