#pragma once

#include <SDL/SDL.h>
#include <stdint.h>

enum class Control {
  UNMAPPED, UP, DOWN, LEFT, RIGHT, NORTH, SOUTH, WEST, EAST, R1, L1, R2, L2, START, SELECT, MENU, LAST_ITEM,
};

const int TYPE_KEY = 0 << 16;
const int TYPE_BUTTON = 1 << 16;
const int TYPE_HAT = 2 << 16;

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
    const int32_t *words = reinterpret_cast<const int32_t*>(str);
    uint32_t h = 0;
    while (len > 0) {
      int32_t w = *words++;
      if (len < 4) w &= (~0u >> (32 - len*8));
      h += hash(w);
      len -= 4;
    }
    h &= INT32_MAX;
    return h;
  }
};

struct KeyMapTable {
  int32_t numEntries;
  int32_t maxProbes;
  KeyHasher hasher;
  int32_t mappings[0];
};

class InputMapping {
  static const KeyMapTable* const defaultLayout;

  const KeyMapTable *table;
  Control mapRaw(int32_t val) const;
public:
  inline InputMapping(): table(defaultLayout) { }
  inline InputMapping(const KeyMapTable *t): table(t) { }

  inline void setTable(void *tableBuffer) {
    table = reinterpret_cast<const KeyMapTable*>(tableBuffer);
  }

  inline Control mapKey(SDLKey key) const {
    return mapRaw(static_cast<int32_t>(key) | TYPE_KEY);
  }

  inline Control mapButton(int button) const {
    return mapRaw(button | TYPE_BUTTON);
  }

  inline Control mapHatDirection(int directionMask) const {
    return mapRaw(directionMask | TYPE_HAT);
  }
};
