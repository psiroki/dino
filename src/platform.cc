#include "platform.hh"

#include <iostream>

#ifdef DESKTOP
#define USE_JOYSTICK
#endif

#ifdef USE_SDL2
namespace {
  void dumpRendererInfo(const SDL_RendererInfo &info) {
    std::cout << info.name << " flags: " << info.flags << std::endl;
  }
}
#endif

Platform::Platform():
#ifdef USE_SDL2
  window(nullptr),
  renderer(nullptr),
  texture(nullptr),
#endif
  screen(nullptr) { }

// Initialize SDL and create a window with the specified dimensions
SDL_Surface* Platform::initSDL(int w, int h, int o, bool sr, bool fr) {
  width = w;
  height = h;
  orientation = o & 3;
  Uint32 initFlags = SDL_INIT_VIDEO | SDL_INIT_AUDIO;
#ifdef USE_JOYSTICK
  initFlags |= SDL_INIT_JOYSTICK;
#endif
#ifdef USE_GAME_CONTROLLER
  initFlags |= SDL_INIT_GAMECONTROLLER;
#endif
  if (SDL_Init(initFlags) < 0) {
    std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
    return nullptr;
  }
#ifdef USE_JOYSTICK
  if (SDL_NumJoysticks() > 0) {
    SDL_JoystickOpen(0);
  }
#endif

#ifdef USE_SDL2
  int numRenderDrivers = SDL_GetNumRenderDrivers();
  std::cout << "Number of render drivers: " << numRenderDrivers << std::endl;
  int driverToUse = -1;
  for (int i = 0; i < numRenderDrivers; ++i) {
    SDL_RendererInfo info;
    SDL_GetRenderDriverInfo(i, &info);
    std::cout << "#" << i << " ";
    dumpRendererInfo(info);
    if (driverToUse < 0 && info.flags & SDL_RENDERER_ACCELERATED) driverToUse = i;
  }
  softRotate = sr && orientation;
  forceTexture = fr;
  bool fullscreen = (width == 0 || height == 0);
  if (fullscreen) {
    SDL_DisplayMode displayMode;
    if (SDL_GetCurrentDisplayMode(0, &displayMode) != 0) {
      std::cerr << "Failed to get display mode: " << SDL_GetError() << std::endl;
      SDL_Quit();
      return nullptr;
    }
    width = displayMode.w;
    height = displayMode.h;
    std::cout << "Display mode is " << width << "x" << height << std::endl;
  }

  Uint32 windowFlags = SDL_WINDOW_SHOWN | (fullscreen ? SDL_WINDOW_FULLSCREEN : 0);
  window = SDL_CreateWindow(name, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, windowFlags);
  if (!window) {
      std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
      SDL_Quit();
      return nullptr;
  }

  // Create a renderer with vsync enabled, compatible with SDL2's double-buffering
  uint32_t flags = driverToUse < 0 ? SDL_RENDERER_SOFTWARE : SDL_RENDERER_ACCELERATED;
#ifdef PORTMASTER
  // Let SDL2 choose a driver for us
  driverToUse = -1;
#endif
#ifndef MIYOOA30
  flags |= SDL_RENDERER_PRESENTVSYNC;
#endif
  renderer = SDL_CreateRenderer(window, driverToUse, flags);
  if (!renderer) {
      std::cerr << "Failed to create renderer: " << SDL_GetError() << std::endl;
      SDL_DestroyWindow(window);
      SDL_Quit();
      return nullptr;
  }
  SDL_RendererInfo info;
  SDL_GetRendererInfo(renderer, &info);
  std::cout << "Renderer: ";
  dumpRendererInfo(info);
  if (forceTexture || !(!orientation || softRotate)) {
    std::cout << "Will use texture streaming" << std::endl;
  }

  // Optional: get the window surface if needed (e.g., for software rendering)
  if (!forceTexture && (!orientation || softRotate)) {
    screen = SDL_GetWindowSurface(window);
    if (!screen) {
      std::cerr << "Failed to create screen surface: " << SDL_GetError() << std::endl;
      SDL_DestroyRenderer(renderer);
      SDL_DestroyWindow(window);
      SDL_Quit();
      return nullptr;
    }
    if (orientation) {
      int sw = orientation & 1 ? height : width;
      int sh = orientation & 1 ? width : height;
      rotated = SDL_CreateRGBSurface(0, sw, sh, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
      if (!rotated) {
        std::cerr << "Failed to create screen surface: " << SDL_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return nullptr;
      }
      makeOpaque(rotated);
    }
  } else {
    int sw = orientation & 1 ? height : width;
    int sh = orientation & 1 ? width : height;
    // Create the screen surface for software rendering (always use the unrotated dimensions)
    screen = SDL_CreateRGBSurface(0, sw, sh, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    if (!screen) {
      std::cerr << "Failed to create screen surface: " << SDL_GetError() << std::endl;
      SDL_DestroyRenderer(renderer);
      SDL_DestroyWindow(window);
      SDL_Quit();
      return nullptr;
    }
    makeOpaque(screen);

    // Create a texture to present the final image on screen
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, sw, sh);
    if (!texture) {
      std::cerr << "Failed to create texture: " << SDL_GetError() << std::endl;
      SDL_FreeSurface(screen);
      SDL_DestroyRenderer(renderer);
      SDL_DestroyWindow(window);
      SDL_Quit();
      return nullptr;
    }

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_NONE);
  }

  SDL_SetSurfaceBlendMode(screen, SDL_BLENDMODE_NONE);

  return softRotate ? rotated : screen;
#else
  bool fullscreen = width == 0 || height == 0;
  if (fullscreen) {
    const SDL_VideoInfo *videoInfo = SDL_GetVideoInfo();
    width = videoInfo->current_w;
    height = videoInfo->current_h;
  }

  Uint32 videoModeFlags = SDL_DOUBLEBUF | SDL_HWSURFACE;
  if (fullscreen) videoModeFlags |= SDL_FULLSCREEN;
  screen = SDL_SetVideoMode(width, height, 32, videoModeFlags);
  if (screen == nullptr) {
    std::cerr << "Failed to set video mode: " << SDL_GetError() << std::endl;
    return nullptr;
  }
  if (orientation || fr) {
    int sw = orientation & 1 ? height : width;
    int sh = orientation & 1 ? width : height;
    rotated = screen;
    screen = SDL_CreateRGBSurface(
      SDL_SWSURFACE,
      sw, // Width of the image
      sh, // Height of the image
      32, // Bits per pixel (8 bits per channel * 4 channels = 32 bits)
      rotated->format->Rmask,
      rotated->format->Gmask,
      rotated->format->Bmask,
      rotated->format->Amask
      // 0x00ff0000, // Red mask
      // 0x0000ff00, // Green mask
      // 0x000000ff, // Blue mask
      // 0xff000000  // Alpha mask
    );
  } else {
    rotated = nullptr;
  }

  SDL_WM_SetCaption(name, nullptr);

  return screen;
#endif
}

SDL_Surface* Platform::displayFormatAndFree(SDL_Surface *src) {
  SDL_Surface *newSurface = displayFormat(src);
  SDL_FreeSurface(src);
  return newSurface;
}

SDL_Surface* Platform::displayFormat(SDL_Surface *src) {
#ifdef USE_SDL2
  if (!src) {
    std::cerr << "src is null" << std::endl;
    SDL_Quit();
  }

  if (!screen) {
    std::cerr << "screen is null" << std::endl;
    SDL_Quit();
  }

  if (!screen->format) {
    std::cerr << "screen->format is null" << std::endl;
    SDL_Quit();
  }

  SDL_Surface *result = SDL_ConvertSurfaceFormat(
      src,
      SDL_PIXELFORMAT_ARGB8888,
      0
  );
  SDL_BlitSurface(src, nullptr, result, nullptr);
  return result;
#else
  return SDL_DisplayFormatAlpha(src);
#endif
}

SDL_Surface* Platform::createSurface(int width, int height) {
  return SDL_CreateRGBSurface(
#ifdef USE_SDL2
    0,
#else
    SDL_SWSURFACE,
#endif
    width, // Width of the image
    height, // Height of the image
    32, // Bits per pixel (8 bits per channel * 4 channels = 32 bits)
    screen->format->Rmask,
    screen->format->Gmask,
    screen->format->Bmask,
    ~(screen->format->Rmask|screen->format->Gmask|screen->format->Bmask)  // the rest is alpha
    // 0x00ff0000, // Red mask
    // 0x0000ff00, // Green mask
    // 0x000000ff, // Blue mask
    // 0xff000000  // Alpha mask
  );
}

void Platform::makeOpaque(SDL_Surface *s, bool opaque) {
#ifdef USE_SDL2
  SDL_SetSurfaceBlendMode(s, opaque ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND);
#else
  SDL_SetAlpha(s, opaque ? 0 : SDL_SRCALPHA, 255);
#endif
}

void Platform::present() {
#ifdef USE_SDL2
  if (!forceTexture && (!orientation || softRotate)) {
    if (softRotate && (orientation & 1)) {
      SurfaceLocker r(rotated);
      SurfaceLocker s(screen);
      bool flip = orientation & 2;
      int increment = flip ? -s.pb.pitch : s.pb.pitch;
      uint32_t *base = s.pb.pixels + (flip ? (r.pb.width - 1) * s.pb.pitch : 0);
      for (int y = 0; y < r.pb.height; ++y) {
        uint32_t *line = r.pb.pixels + y * r.pb.pitch;
        uint32_t *column = base + (flip ? y : r.pb.height - y - 1);
        for (int x = 0; x < r.pb.width; ++x) {
          *column = *line;
          ++line;
          column += increment;
        }
      }
    }
    //SDL_RenderPresent(renderer);
    SDL_UpdateWindowSurface(window);
  } else {
    // uint32_t *pixels = nullptr;
    // int pitch;
    // SurfaceLocker lock(screen);
    // SDL_LockTexture(texture, nullptr, reinterpret_cast<void**>(&pixels), &pitch);
    // pitch >>= 2;
    // for (int y = 0; y < lock.pb.height; ++y) {
    //   memcpy(pixels + pitch*y, lock.pb.pixels + y*lock.pb.pitch, lock.pb.width*4);
    // }
    // SDL_UnlockTexture(texture);
    SDL_UpdateTexture(texture, nullptr, screen->pixels, screen->pitch);
    SDL_Rect dst {
      .x = (width - screen->w) >> 1,
      .y = (height - screen->h) >> 1,
      .w = screen->w,
      .h = screen->h,
    };
    SDL_RenderClear(renderer);
    SDL_RenderCopyEx(renderer, texture, nullptr, &dst, orientation * 90.0, nullptr, SDL_FLIP_NONE);
    SDL_RenderPresent(renderer);
  }
#else
  if (orientation) {
    SurfaceLocker r(rotated);
    SurfaceLocker s(screen);
    if (orientation & 1) {
      bool flip = orientation & 2;
      int increment = flip ? -r.pb.pitch : r.pb.pitch;
      uint32_t *base = r.pb.pixels + (flip ? (s.pb.width - 1) * r.pb.pitch : 0);
      uint32_t *src = s.pb.pixels;
      for (int y = 0; y < s.pb.height; ++y) {
        uint32_t *line = src;
        uint32_t *column = base + (flip ? y : s.pb.height - y - 1);
        for (int x = 0; x < s.pb.width; ++x) {
          *column = *line;
          ++line;
          column += increment;
        }
        src += s.pb.pitch;
      }
    } else {
      uint32_t *src = s.pb.pixels;
      uint32_t *dst = r.pb.pixels + (r.pb.height - 1) * r.pb.pitch + r.pb.width;
      for (int y = 0; y < s.pb.height; ++y) {
        uint32_t *srcLine = src;
        uint32_t *dstLine = dst;
        for (int x = 0; x < s.pb.width; ++x) {
          *--dstLine = *srcLine++;
        }
        src += s.pb.pitch;
        dst -= r.pb.pitch;
      }
    }
  } else if (rotated) {
    SurfaceLocker r(rotated);
    SurfaceLocker s(screen);
    uint32_t *src = s.pb.pixels;
    uint32_t *dst = r.pb.pixels;
    for (int y = 0; y < s.pb.height; ++y) {
      uint32_t *srcLine = src;
      uint32_t *dstLine = dst;
      for (int x = 0; x < s.pb.width; ++x) {
        *dstLine++ = *srcLine++;
      }
      src += s.pb.pitch;
      dst += r.pb.pitch;
    }
  }
  SDL_Flip(rotated ? rotated : screen);
#endif
}


#ifdef _WIN32
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN

// Turn off our own FIXED definition
#ifdef FIXED
#undef FIXED
#endif
#include <windows.h>

extern int main(int argc, char **argv);

int __stdcall WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int sw) {
  return main(__argc, __argv);
}
#endif
