#pragma once

#include <time.h>
#include <stdint.h>

uint32_t micros();
uint32_t microDiff(uint32_t start, uint32_t end);

class Random {
  uint64_t seed;
public:
  Random(uint64_t seed=0);

  uint64_t operator()();

  uint64_t operator()(int n);

  float fraction();
};

class Timestamp {
  timespec time;
  static float secondsDiff(const timespec &a, const timespec &b);
public:
  Timestamp();
  void reset();
  void resetWithDelta(float deltaSeconds);
  float elapsedSeconds(bool reset = false);
  float secondsTo(const Timestamp &other);
};

struct KeyHasher {
  int32_t m, n, o, s;

  inline KeyHasher() {}
  
  inline KeyHasher(int32_t m, int32_t n, int32_t o, int32_t s): m(m), n(n), o(o), s(s) {}

  inline uint32_t hash(int32_t val) const {
    uint32_t hash = val * m;
    hash += (val * n) >> (s / 2);
    hash ^= (val * o) >> s;
    return hash;
  }

  inline uint32_t hash(const char *str) const {
    int32_t len = strnlen(str, 256);
    const uint32_t *words = reinterpret_cast<const uint32_t*>(str);
    uint32_t h = 0;
    while (len > 0) {
      uint32_t w = *words++;
      if (len < 4) w &= (~0u >> (32 - len*8));
      uint32_t thisHash = hash(w);
      h += thisHash;
      len -= 4;
    }
    h &= INT32_MAX;
    return h;
  }
};
