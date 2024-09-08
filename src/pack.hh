#pragma once

#include <stdint.h>
#include <string>
#include <SDL/SDL.h>

#include "util.hh"

struct BufferView {
  void *buffer;
  uint32_t sizeInBytes;

  inline uint8_t* atOffset(uint32_t offset) {
    return reinterpret_cast<uint8_t*>(buffer) + offset;
  }

  inline const uint8_t* atOffset(uint32_t offset) const {
    return reinterpret_cast<const uint8_t*>(buffer) + offset;
  }

  inline void allocateAndCopy(const BufferView &other) {
    buffer = new char[other.sizeInBytes];
    sizeInBytes = other.sizeInBytes;
    memcpy(buffer, other.buffer, sizeInBytes);
  }

  inline void release() {
    if (buffer) {
      delete[] reinterpret_cast<char*>(buffer);
      buffer = nullptr;
      sizeInBytes = 0;
    }
  }
};

struct BufferSlice {
  int32_t wordOffset;
  uint32_t sizeInBytes;

  inline void set(const void* ptr, uint32_t size) {
    sizeInBytes = size;
    const uint32_t *p = reinterpret_cast<const uint32_t*>(ptr);
    const uint32_t *t = reinterpret_cast<const uint32_t*>(this);
    wordOffset = p - t;
  }

  void* ptr() {
    uint32_t *t = reinterpret_cast<uint32_t*>(this);
    return t + wordOffset;
  }
};

struct SlicedBuffer {
  static const uint32_t MAGIC = 0x11897253;

  uint32_t magic;
  KeyHasher hasher;
  uint32_t numTableEntries;
  uint32_t numSlices;
  uint32_t maxProbes;
  uint32_t flags;
  uint32_t data[0];
protected:
  inline uint32_t* table() {
    return data;
  }

  inline BufferSlice* slices() {
    return reinterpret_cast<BufferSlice*>(data + numTableEntries * 2);
  }

  inline uint32_t *contents() {
    return data + numTableEntries * 2 + numSlices * sizeof(BufferSlice) / 4;
  }

public:
  BufferView lookup(const char *fn);
  SDL_Surface* loadPNG(const char *fn);
};
