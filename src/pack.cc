#include "pack.hh"
#include "image.hh"

BufferView SlicedBuffer::lookup(const char *fn) {
  uint32_t *t = table();
  BufferSlice *s = slices();
  if (flags&1) {
    for (int i = 0; i < numTableEntries; ++i) {
      if (~t[i*2]) {
        uint32_t h = t[i*2];
        BufferSlice *slice = s + t[i*2+1];
        uint32_t *p = reinterpret_cast<uint32_t*>(slice->ptr());
        uint32_t size = (slice->sizeInBytes + 3) >> 2;
        for (int j = 0; j < size; ++j) {
          p[j] ^= h;
          h += hasher.hash(p[j]);
        }
      }
    }
    flags &= ~1U;
  }
  uint32_t hash = hasher.hash(fn);
  uint32_t index = (hash % numTableEntries) * 2;
  uint32_t sliceIndex = t[index + 1];
  if (t[index] == hash && sliceIndex < numSlices) {
    s += sliceIndex;
    BufferView view { .buffer = s->ptr(), .sizeInBytes = s->sizeInBytes };
    return view;
  } else {
    BufferView view { .buffer = nullptr, .sizeInBytes = 0 };
    return view;
  }
}

SDL_Surface* SlicedBuffer::loadPNG(const char *fn) {
  BufferView view = lookup(fn);
  return loadPNGFromMemory(view.buffer, view.sizeInBytes);
}
