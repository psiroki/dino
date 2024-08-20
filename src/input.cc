#include "input.hh"

namespace {

const uint8_t defaultLayoutBytes[] = {
  0x0f,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x4d,0x00,0x00,0x00,
  0x1c,0x00,0x00,0x00,0x0c,0x00,0x00,0x00,0x65,0x00,0x00,0x00,0x0b,0x00,0x00,0x00,
  0x20,0x00,0x00,0x00,0x0e,0x00,0x00,0x00,0x71,0x00,0x00,0x00,0x0c,0x00,0x00,0x00,
  0x2f,0x01,0x00,0x00,0x09,0x00,0x00,0x00,0x77,0x00,0x00,0x00,0x05,0x00,0x00,0x00,
  0x1b,0x00,0x00,0x00,0x0f,0x00,0x00,0x00,0x13,0x01,0x00,0x00,0x04,0x00,0x00,0x00,
  0x0d,0x00,0x00,0x00,0x0d,0x00,0x00,0x00,0x30,0x01,0x00,0x00,0x0a,0x00,0x00,0x00,
  0x61,0x00,0x00,0x00,0x07,0x00,0x00,0x00,0x64,0x00,0x00,0x00,0x08,0x00,0x00,0x00,
  0x11,0x01,0x00,0x00,0x01,0x00,0x00,0x00,0x14,0x01,0x00,0x00,0x03,0x00,0x00,0x00,
  0x73,0x00,0x00,0x00,0x06,0x00,0x00,0x00,0x12,0x01,0x00,0x00,0x02,0x00,0x00,0x00,
};

}

const KeyMapTable* const InputMapping::defaultLayout = reinterpret_cast<const KeyMapTable*>(defaultLayoutBytes);

Control InputMapping::mapRaw(int32_t val) const {
  uint32_t hash = table->hasher.hash(val);
  for (int i = table->maxProbes; i > 0; --i, ++hash) {
    uint32_t baseIndex = 2*(hash % table->numEntries);
    const int32_t *entry = table->mappings + baseIndex;
    if (entry[0] == val) return static_cast<Control>(entry[1]);
  }
  return Control::UNMAPPED;
}
