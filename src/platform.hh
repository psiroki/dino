#pragma once 

#ifdef USE_SDL2
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#else
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>
#endif

#ifndef USE_SDL2
#undef USE_GAME_CONTROLLER
#endif

class Platform {
  SDL_Surface *screen;
  SDL_Surface *rotated;
  int width, height;
  int orientation;
  const char *name;
#ifdef USE_SDL2
  bool softRotate;
  bool forceTexture;
  SDL_Window* window;
  SDL_Renderer* renderer;
  SDL_Texture* texture;        // Texture to display the final surface
#endif
public:
  Platform();
  
  inline void setName(const char *newName) {
    name = newName;
  }

  SDL_Surface *initSDL(int width, int height, int orientation = 0, bool softRotate = true, bool forceTexture = false);
  SDL_Surface* displayFormat(SDL_Surface *src);
  SDL_Surface* displayFormatAndFree(SDL_Surface *src);
  SDL_Surface* createSurface(int width, int height);
  void makeOpaque(SDL_Surface *s, bool opaque = true);
  void present();
};

inline SDL_Rect makeRect(int x, int y, int w = 0, int h = 0) {
  return {
    .x = static_cast<Sint16>(x),
    .y = static_cast<Sint16>(y),
    .w = static_cast<Uint16>(w),
    .h = static_cast<Uint16>(h),
  };
};


struct PixelBuffer {
  /// Pointer to the 32 bit pixels
  uint32_t *pixels;
  /// Width in pixels
  int width;
  /// Height in pixels
  int height;
  /// Word pitch
  int pitch;

  inline PixelBuffer(int width = 0, int height = 0, int pitch = 0, uint32_t *pixels = nullptr):
    width(width),
    height(height),
    pitch(pitch),
    pixels(pixels) {
  }

  inline PixelBuffer(SDL_Surface *s) {
    *this = s;
  }

  inline void operator=(SDL_Surface *s) {
    if (s) {
      width = s->w;
      height = s->h;
      pitch = s->pitch >> 2;
      pixels = reinterpret_cast<uint32_t*>(s->pixels);
    } else {
      width = height = pitch = 0;
      pixels = nullptr;
    }
  }

  inline PixelBuffer cropped(int x1, int y1, int x2, int y2) {
    PixelBuffer result(*this);
    result.pixels += x1 + y1 * pitch;
    result.width = x2 - x1;
    result.height = y2 - y1;
    return result;
  }
};

struct SurfaceLocker {
  SDL_Surface *surface;
  PixelBuffer pb;

  inline SurfaceLocker(SDL_Surface *surface=nullptr): surface(surface), pb(nullptr) {
    if (surface && SDL_MUSTLOCK(surface)) {
      SDL_LockSurface(surface);
    }
    pb = surface;
  }

  inline void unlock() {
    if (surface) {
      if (SDL_MUSTLOCK(surface)) {
        SDL_LockSurface(surface);
      }
      pb = surface = nullptr;
    }
  }

  inline SDL_Surface* operator=(SDL_Surface *s) {
    unlock();
    surface = s;
    if (surface && SDL_MUSTLOCK(surface)) {
      SDL_LockSurface(surface);
    }
    pb = surface;
    return surface;
  }

  inline ~SurfaceLocker() {
    unlock();
  }
};
