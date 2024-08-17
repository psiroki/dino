#pragma once

#include <SDL/SDL.h>
#include <memory>
#include <string.h>
#include <stdint.h>

namespace perf { class Window; }

class PerfTextOverlay {
  friend class perf::Window;
  char* __attribute__((aligned(32))) buffer;
  // numColumns is always divisible by 4
  int numColumns;
  int numChars;
  int numRows;
  uint32_t color;
  int orientation;

  void drawOverlayNormal(SDL_Surface *surface);
  void drawOverlay180(SDL_Surface *surface);
public:
  PerfTextOverlay(int pixelWidth, int pixelHeight, int orientation);
  ~PerfTextOverlay();

  inline void setColor(uint32_t newColor) {
    color = newColor;
  }

  inline char* getBuffer() {
    return buffer;
  }

  inline int getNumColumns() {
    return numColumns;
  }

  inline int getNumRows() {
    return numRows;
  }

  inline void clear() {
    memset(buffer, 0, numChars);
  }

  void write(uint32_t x, uint32_t y, const char *text);

  /// Draws the text overlay on the surface.
  /// The surface is assumed to be locked,
  /// and to be at least the size that was given
  /// in the constructor (it can be larger though).
  /// The pixel format should be 32 bits
  void drawOverlay(SDL_Surface *surface);
};

namespace perf {

  inline const char* const endl = "\n";

  struct setw {
    const int w;

    inline setw(int val) : w(val) {}
  };

  class Window {
    char *buffer, *cursor;
    int cursorRow, cursorColumn;
    int numColumns;
    int pitch;
    int numRows;
    int width;

    void scrollUp(int amount);
    void finishLine(bool force);
    void writeString(const char *str);
    void align(char *str, int32_t size);
  public:
    Window(PerfTextOverlay &overlay, int x, int y, int w, int h);

    void clear();
    Window& operator<<(const char *s);
    Window& operator<<(uint32_t n);
    Window& operator<<(int32_t n);
    Window& operator<<(int64_t n);
    Window& operator<<(bool b);
    Window& operator<<(const setw &newWidth);
  };

}
