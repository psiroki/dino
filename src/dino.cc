#include <SDL/SDL.h>
#include <iostream>
#include <fstream>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "util.hh"
#include "image.hh"
#include "input.hh"
#include "perftext.hh"

#ifdef __EMSCRIPTEN__
// no blowup
#else
#ifdef DESKTOP
#define BLOWUP 2
#endif
#endif

#ifdef MIYOO
#define BLOWUP 1
#define FLIP
#endif

#ifdef RG35XX
#define BLOWUP 1
#endif

#if defined(DESKTOP)
static const char * const LAYOUT_FILE = "PC";
#elif defined(MIYOO)
static const char * const LAYOUT_FILE = "MiyooMini";
#elif defined(BITTBOY)
static const char * const LAYOUT_FILE = "Bittboy";
#elif defined(RG35XX22)
static const char * const LAYOUT_FILE = "RG35XX22";
#elif defined(RG35XX)
static const char * const LAYOUT_FILE = "RG35XX";
#else
static const char * const LAYOUT_FILE = nullptr;
#endif


const uint32_t FP_SHIFT = 16;
const uint32_t DP_SHIFT = 17;

const int JUMPS = 2;
const int MAX_OBSTACLES = 16;

inline int max(int a, int b) {
  return a > b ? a : b;
}

typedef int (*MonoSampleGenerator)(uint32_t sampleIndex);

struct SoundBuffer {
  uint32_t *samples;
  uint32_t numSamples;

  inline SoundBuffer(): samples(0), numSamples(0) { }
  ~SoundBuffer();

  void resize(uint32_t newNumSamples);
  void generateMono(uint32_t newNumSamples, MonoSampleGenerator gen);
};

SoundBuffer::~SoundBuffer() {
  if (samples) delete[] samples;
}

void SoundBuffer::resize(uint32_t newNumSamples) {
  if (newNumSamples != numSamples) {
    if (samples) delete[] samples;
    numSamples = newNumSamples;
    samples = numSamples ? new uint32_t[newNumSamples] : nullptr;
  }
}

void SoundBuffer::generateMono(uint32_t newNumSamples, MonoSampleGenerator gen) {
  resize(newNumSamples);
  for (uint32_t i = 0; i < numSamples; ++i) {
    int s = gen(i) & 0xFFFF;
    samples[i] = (s << 16) | s;
  }
}

struct MixChannel {
  const SoundBuffer *buffer;
  uint64_t timeStart;

  bool isOver(uint64_t audioTime) {
    return !buffer || timeStart < audioTime && (timeStart + buffer->numSamples) < audioTime;
  }
};

class Mixer {
  static const int maxNumChannels = 16;
  static const int soundQueueSize = 16;

  uint64_t audioTime[4];
  Stopwatch times[4];
  MixChannel soundsToAdd[soundQueueSize];
  int soundRead;
  int soundWrite;
  MixChannel channels[maxNumChannels];
  int numChannelsUsed;
  int currentStopwatch;
public:
  inline Mixer(): audioTime { 0, 0, 0, 0 }, currentStopwatch(0), soundRead(0), soundWrite(0) { }
  void audioCallback(uint8_t *stream, int len);
  void playSound(const SoundBuffer *buffer);
};

void Mixer::audioCallback(uint8_t *stream, int len) {
  uint64_t time = audioTime[currentStopwatch];
  int numSamples = len / 4;

  int nextWatch = (currentStopwatch + 1) & 3;
  times[nextWatch].reset();
  audioTime[nextWatch] = time + numSamples;
  currentStopwatch = nextWatch;

  // remove finished channels
  for (int i = numChannelsUsed - 1; i >= 0; --i) {
    if (channels[i].isOver(time)) {
      if (i < numChannelsUsed - 1) {
        // swap with last
        channels[i] = channels[numChannelsUsed - 1];
      }
      --numChannelsUsed;
    }
  }
  // add new channels
  while (soundRead != soundWrite) {
    channels[numChannelsUsed++] = soundsToAdd[soundRead];
    soundRead = (soundRead + 1) & (soundQueueSize - 1);
  }

  uint32_t *s = reinterpret_cast<uint32_t*>(stream);
  for (int i = 0; i < numSamples; ++i) {
    int mix[2] { 0, 0 };
    for (int j = 0; j < numChannelsUsed; ++j) {
      MixChannel &ch(channels[j]);
      uint64_t start = ch.timeStart;
      if (start <= time && !ch.isOver(time)) {
        uint32_t sampleIndex = time - start;
        uint32_t sample = ch.buffer->samples[sampleIndex];
        for (int k = 0; k < 2; ++k) {
          mix[k] += static_cast<int16_t>(sample >> (k * 16));
        }
      }
    }
    for (int k = 0; k < 2; ++k) {
      if (mix[k] > 32767) mix[k] = 32767;
      if (mix[k] < -32768) mix[k] = -32768 & 0xffff;
    }
    *s++ = (mix[1] << 16) | mix[0];
    ++time;
  }
}

void Mixer::playSound(const SoundBuffer *buffer) {
  MixChannel &ch(soundsToAdd[soundWrite]);
  ch.buffer = buffer;
  int w = currentStopwatch;
  ch.timeStart = audioTime[w] + times[w].elapsedSeconds() * 44100;
  soundWrite = (soundWrite + 1) & (soundQueueSize - 1);
}

struct PixelPtr {
  uint32_t *pixel;
  int32_t pixelPitch;

  inline PixelPtr(SDL_Surface *s): pixel(static_cast<uint32_t*>(s->pixels)), pixelPitch(s->pitch >> 2) { }

  inline void nextPixel() {
    ++pixel;
  }

  inline void nextLine() {
    pixel += pixelPitch;
  }

  inline uint32_t& operator *() {
    return *pixel;
  }

  inline operator uint32_t*() {
    return pixel;
  }
};

struct Collider {
  int x, y;
  int w, h;

  bool overlaps(const Collider &other) const;
};

bool Collider::overlaps(const Collider &other) const {
  int x1 = x - w / 2;
  int x2 = x + w / 2;
  int y1 = y - h / 2;
  int y2 = y + h / 2;

  int ox1 = other.x - other.w / 2;
  int ox2 = other.x + other.w / 2;
  int oy1 = other.y - other.h / 2;
  int oy2 = other.y + other.h / 2;

  return !(x2 <= ox1 || y2 <= oy1 || x1 >= ox2 || y1 >= oy2);
}

struct DynamicCollider: public Collider {
  int lastX, lastY;
  bool grounded;

  void update() {
    grounded = false;
    // gravity
    y += (1 << FP_SHIFT) / 2;
    int dx = x - lastX;
    int dy = y - lastY;
    lastX = x;
    lastY = y;
    x += dx;
    y += dy;
    if (y > -h/2) {
      y = -h/2;
      grounded = true;
    }
  }
};

struct Appearance {
  static const int SHADOW = 1;
  static const int COVER6 = 2;

  uint32_t color;
  SDL_Surface *surface;
  int frameWidth;
  int frameHeight;
  int frameX;
  int frameY;
  int xOffset;
  int yOffset;
  int coverWidth;
  int coverHeight;
  int flags;

  Appearance(): surface(nullptr), frameWidth(0), frameHeight(0), xOffset(0), yOffset(0), flags(SHADOW) { }
};

struct Dino {
  DynamicCollider collider;
  Appearance appearance;

  Dino() {
    collider.w = 24 << FP_SHIFT;
    collider.h = 24 << FP_SHIFT;
    collider.x = 0;
    collider.y = -collider.h / 2;
    collider.lastX = collider.x;
    collider.lastY = collider.y;
  }

  inline void duckEnabled(bool val) {
    collider.h = (val ? 12 : 24) << FP_SHIFT;
  }

  inline int duckHeight() {
    return 16 << FP_SHIFT;
  }
};

struct Obstacle {
  Collider collider;
  Appearance appearance;

  Obstacle() {
    reset();
  }

  /// Resets to the given height, 0 means default
  void reset(int width = 0, int height=0);
};

void Obstacle::reset(int width, int height) {
  if (!height) height = 32 << FP_SHIFT;
  if (!width) width = 16 << FP_SHIFT;
  collider.w = width;
  collider.h = height;
  collider.x = (640 << FP_SHIFT) + collider.w / 2;
  collider.y = -collider.h / 2;
}

enum class Activity { playing, stopping };

void callAudioCallback(void *userdata, uint8_t *stream, int len);

struct ControlState {
  bool controlState[static_cast<int>(Control::LAST_ITEM)];

  inline bool& operator [](int index) {
    return controlState[index];
  }

  inline bool& operator [](Control control) {
    return controlState[static_cast<int>(control)];
  }
};

class DinoJump {
  friend void callAudioCallback(void *userdata, uint8_t *stream, int len);
#if BLOWUP
  SDL_Surface *realScreen;
#endif
  SDL_Surface *screen;
  SDL_Surface *vita;
  SDL_Surface *bg;
  SDL_Surface *ground;
  SDL_Surface *shadow;
  SDL_Surface *wideShadow;
  SDL_Surface *blimp;
  SDL_Surface *building;

  PerfTextOverlay overlay;

  SoundBuffer jump;
  SoundBuffer step;
  Mixer mixer;

  bool running;

  int frame;
  int lastStepFrame;
  int backgroundOffset;
  Random random;
  Dino dino;
  int32_t lastHatBits;
  ControlState controlState;
  InputMapping inputLayout;
  char inputLayoutBytes[1024];
  int cx, cy;
  int jumpsLeft;
  bool duck;
  Obstacle obstacles[MAX_OBSTACLES];
  int numObstacles;
  int difficulty;
  Activity activity;
  Stopwatch stopEnd;

  void drawCollider(const Collider &c, const Appearance &appearance);
  void drawGround();
  void render();
  void update();
  void handleKeyEvent(const SDL_Event &event);
  void handleJoyHat(int32_t hatBits);
  void handleJoyButton(uint8_t button, uint8_t value);
  void handleControlEvent(Control control, bool down);
  inline uint32_t randomBrightColor() {
    return SDL_MapRGB(screen->format, random() & 255 | 128, random() & 255 | 128, random() & 255 | 128);
  }
  void audioCallback(uint8_t *stream, int len);
public:
  inline DinoJump():
      screen(nullptr),
      running(false),
      frame(0),

      lastHatBits(0),
      activity(Activity::playing),
      random(micros()),
      cx(160 << FP_SHIFT),
      cy(320 << FP_SHIFT),
      jumpsLeft(JUMPS),
      duck(false),
      numObstacles(0),
      difficulty(1),
      overlay(320, 240, 0) {
  }
  void init();
  void run();
  void loop();
};

void DinoJump::init() {
  if (screen) return;

  memset(&controlState, 0, sizeof(controlState));
  if (LAYOUT_FILE) {
    char path[256];
    snprintf(path, sizeof(path), "assets/%s.layout", LAYOUT_FILE);
    std::cerr << "Using " << path << " for input layout" << std::endl;
    std::ifstream file(path, std::ios::binary);
    if (file.is_open()) {
      file.read(inputLayoutBytes, sizeof(inputLayoutBytes));

      if (file.gcount() >= sizeof(inputLayoutBytes)) {
          std::cerr << "Layout file is too big" << std::endl;
      }
      file.close();
    } else {
      std::cerr << "Could not load layout file" << std::endl;
    }
    inputLayout.setTable(inputLayoutBytes);
  }

  jump.generateMono(10000, [](uint32_t index) -> int {
    int i2 = index * index / 1000 & 127;
    int32_t s = abs(i2 - 64) - 32;
    if (s < -24) s = -24;
    if (s > 24) s = 24;
    int vol = 10000 - index;
    return s * vol / 10;
  });

  step.generateMono(600, [](uint32_t index) -> int {
    int angle = index % 600;
    int32_t s = angle < 300 ? -16 : 16;
    int vol = 10000 - index;
    return s * vol >> 4;
  });

  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK);

  if (SDL_NumJoysticks() > 0) {
    SDL_JoystickOpen(0);
  }

  SDL_AudioSpec desired;
  desired.freq = 44100;
  desired.format = AUDIO_S16LSB;
  desired.channels = 2;
  desired.samples = 4096;
  desired.userdata = this;
  desired.callback = callAudioCallback;
  SDL_OpenAudio(&desired, nullptr);
  SDL_PauseAudio(0);
  uint32_t flags = 0;
#if BLOWUP
  realScreen = SDL_SetVideoMode(320 << BLOWUP, 240 << BLOWUP, 32, 0);
  screen = SDL_CreateRGBSurface(0, 320, 240, 32,
      realScreen->format->Rmask, realScreen->format->Gmask, realScreen->format->Bmask,
      realScreen->format->Amask);
#else
#ifdef __EMSCRIPTEN__
  uint32_t additionalFlags = 0;
#else
  uint32_t additionalFlags = SDL_DOUBLEBUF | SDL_HWSURFACE;
#endif
  screen = SDL_SetVideoMode(320, 240, 32, flags | additionalFlags);
#endif
  SDL_WM_SetCaption("Dino Jump", nullptr);
  SDL_ShowCursor(false);
  dino.appearance.color = randomBrightColor();
  vita = loadPNG("assets/vita.png");
  dino.appearance.surface = vita;
  dino.appearance.frameWidth = 24;
  dino.appearance.frameX = 4;
  dino.appearance.yOffset = 3;
  bg = loadPNG("assets/sky.png");
  ground = loadPNG("assets/ground.png");
  blimp = loadPNG("assets/blimp.png");
  building = loadPNG("assets/building.png");
  shadow = loadPNG("assets/shadow.png");
  const int widening = 2;
  wideShadow = SDL_CreateRGBSurface(0, shadow->w * widening, shadow-> h, 32,
      shadow->format->Rmask, shadow->format->Gmask, shadow->format->Bmask,
      shadow->format->Amask);

  cy = (screen->h - ground->h + 1) << DP_SHIFT;

  SDL_LockSurface(shadow);
  SDL_LockSurface(wideShadow);

  PixelPtr sp(shadow);
  PixelPtr wsp(wideShadow);

  for (int y = 0; y < shadow->h; ++y) {
    uint32_t *src = sp;
    uint32_t *dst = wsp;
    for (int x = 0; x < shadow->w; ++x) {
      uint32_t p = *src++;
      for (int i = 0; i < widening; ++i) {
        *dst++ = p;
      }
    }
    sp.nextLine();
    wsp.nextLine();
  }

  SDL_UnlockSurface(shadow);
  SDL_UnlockSurface(wideShadow);

  SDL_SetAlpha(shadow, SDL_SRCALPHA, 255);
  SDL_SetAlpha(wideShadow, SDL_SRCALPHA, 255);
}

void DinoJump::loop() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    switch (event.type) {
      case SDL_QUIT:
        running = false;
        break;
      case SDL_KEYDOWN:
      case SDL_KEYUP:
        handleKeyEvent(event);
        break;
      case SDL_JOYBUTTONDOWN:
      case SDL_JOYBUTTONUP:
        handleJoyButton(event.jbutton.button, event.jbutton.state);
        break;
      case SDL_JOYHATMOTION:
        handleJoyHat(event.jhat.value);
        break;
      default:
        break;
    }
  }

  update();

  render();
}

void DinoJump::run() {
  running = true;

  while (running) {
    Stopwatch frameStart;

    loop();

    int32_t msLeft = 1000/60 - frameStart.elapsedSeconds()*1000.0f;
    if (msLeft > 0)
      SDL_Delay(msLeft);
  }

  // Clean up
  SDL_Quit();
}

void callAudioCallback(void *userData, uint8_t *stream, int len) {
  static_cast<DinoJump*>(userData)->mixer.audioCallback(stream, len);
}

void DinoJump::handleJoyHat(int32_t hatBits) {
  for (int i = 0; i < 4; ++i) {
    int32_t mask = 1 << i;
    int32_t bit = hatBits & mask;
    if (bit != (lastHatBits & mask)) {
      Control control = inputLayout.mapHatDirection(mask);
      handleControlEvent(control, bit);
    }
  }
  lastHatBits = hatBits;
}

void DinoJump::handleJoyButton(uint8_t button, uint8_t value) {
  Control control = inputLayout.mapButton(button);
  handleControlEvent(control, value);
}

void DinoJump::handleKeyEvent(const SDL_Event &event) {
  SDLKey key = event.key.keysym.sym;
  Control control = inputLayout.mapKey(key);
  handleControlEvent(control, event.type == SDL_KEYDOWN);
}

void DinoJump::handleControlEvent(Control control, bool down) {
  if (down) {
    if (controlState[static_cast<int>(control)]) return;
    controlState[static_cast<int>(control)] = true;
    if (control == Control::MENU || controlState[Control::START] && controlState[Control::SELECT]) running = false;
    if ((control == Control::UP || control == Control::SOUTH || control == Control::EAST) && !duck && jumpsLeft > 0) {
      dino.collider.lastY = dino.collider.y + (12 << FP_SHIFT);
      --jumpsLeft;
      mixer.playSound(&jump);
    }
    if (control == Control::R1 || control == Control::R2) ++difficulty;
    if ((control == Control::L1 || control == Control::L2) && difficulty > 1) --difficulty;
  } else {
    if (!controlState[static_cast<int>(control)]) return;
    controlState[static_cast<int>(control)] = false;
  }
  if (control == Control::DOWN || control == Control::WEST) {
    duck = down;
  }
}

void DinoJump::update() {
  if (activity == Activity::playing) {
    dino.duckEnabled(duck);
    if (duck && dino.collider.y < 0)
      dino.collider.lastY = dino.collider.y - (12 << FP_SHIFT);
    dino.collider.update();
    if (dino.collider.grounded) jumpsLeft = JUMPS;
    int expObstacles = difficulty / 3 + 1;
    if (numObstacles < expObstacles && numObstacles < MAX_OBSTACLES) {
      int maxX = 0;
      for (int i = 0; i < numObstacles; ++i) {
        int x = obstacles[i].collider.x;
        if (x > maxX) maxX = x;
      }
      if (!numObstacles || maxX < (640 << FP_SHIFT) - ((640 << FP_SHIFT) / expObstacles)) {
        bool duckable = random()&4;
        Obstacle &obstacle(obstacles[numObstacles]);
        Appearance &appearance(obstacle.appearance);
        obstacle.reset(
            (duckable ? 72 : random(2*difficulty)*16+16) << FP_SHIFT,
            (duckable ? 48 : random(2*difficulty)*16+16) << FP_SHIFT);
        if (duckable) {
          obstacle.collider.y -= dino.duckHeight() * 5 / 4;
          appearance.surface = blimp;
          appearance.frameWidth = 0;
          appearance.frameHeight = 0;
          appearance.yOffset = -2;
          appearance.flags &= ~Appearance::COVER6;
        } else {
          appearance.surface = building;
          appearance.frameWidth = 12;
          appearance.frameHeight = 12;
          appearance.frameX = 0;
          appearance.frameY = 0;
          appearance.flags |= Appearance::COVER6;
          appearance.coverWidth = max(3, (obstacle.collider.w+(24 << DP_SHIFT)) / 12 >> DP_SHIFT);
          appearance.coverHeight = max(2, (obstacle.collider.h+(12 << DP_SHIFT)) / 12 >> DP_SHIFT);
          appearance.yOffset = 0;
        }
        appearance.color = randomBrightColor();
        ++numObstacles;
      }
    }

    const int speed = difficulty + 4;

    for (int i = 0; i < numObstacles; ++i) {
      obstacles[i].collider.x -= speed << FP_SHIFT;
    }

    backgroundOffset -= speed << FP_SHIFT;
    
    for (int i = 0; i < numObstacles; ++i) {
      const Collider &c(obstacles[i].collider);
      int x2 = (c.x + (c.w >> 1) + cx) >> DP_SHIFT;
      if (x2 < 0) {
        if (i < numObstacles - 1) {
          obstacles[i] = obstacles[numObstacles - 1];
        }
        --i;
        --numObstacles;
      }
    }

    for (int i = 0; i < numObstacles; ++i) {
      const Collider &c(obstacles[i].collider);
      if (c.overlaps(dino.collider)) {
        activity = Activity::stopping;
        stopEnd.resetWithDelta(0.5f);
      }
    }
    int stepFrame = (frame % 24 >> 2);
    dino.appearance.frameX = (duck ? 17 : 4) + stepFrame;
    if (lastStepFrame != stepFrame && stepFrame % 3 == 0 &&
        (dino.collider.y+dino.collider.h/2) > (-1 << FP_SHIFT)) {
      mixer.playSound(&step);
    }
    lastStepFrame = stepFrame;
  } else if (activity == Activity::stopping) {
    dino.appearance.frameX = 13 + (frame % 6 >> 1);
    if (stopEnd.elapsedSeconds() > 0.0f) {
      activity = Activity::playing;
      numObstacles = 0;
    }
  }
  ++frame;
}

void DinoJump::drawCollider(const Collider &c, const Appearance &appearance) {
  int centerX = c.x + cx;
  int centerY = c.y + cy;
  SDL_Surface *surface = appearance.surface;
  if (appearance.flags & Appearance::SHADOW) {
    SDL_Surface *shadowToUse = (c.w >> DP_SHIFT) > shadow->w ? wideShadow : shadow;
    int w = shadowToUse->w;
    int h = shadowToUse->h;
    SDL_Rect shadowDst {
      .x = static_cast<Sint16>((centerX >> DP_SHIFT) - w / 2),
      .y = static_cast<Sint16>((cy >> DP_SHIFT) - h / 2),
    };
    SDL_BlitSurface(shadowToUse, nullptr, screen, &shadowDst);
  }
  if (!surface) {
    int x1 = (centerX - (c.w >> 1)) >> DP_SHIFT;
    int y1 = (centerY - (c.h >> 1)) >> DP_SHIFT;
    int x2 = (centerX + (c.w >> 1)) >> DP_SHIFT;
    int y2 = (centerY + (c.h >> 1)) >> DP_SHIFT;
    SDL_Rect r {
      .x = static_cast<Sint16>(x1),
      .y = static_cast<Sint16>(y1),
      .w = static_cast<Uint16>(x2 - x1),
      .h = static_cast<Uint16>(y2 - y1),
    };
    SDL_FillRect(screen, &r, appearance.color);
  } else {
    int fw = appearance.frameWidth;
    int fh = appearance.frameHeight;
    int fx = fw ? appearance.frameX : 0;
    int fy = fh ? appearance.frameY : 0;
    int w = fw ? fw : surface->w;
    int h = fh ? fh : surface->h;
    int cw = 1;
    int ch = 1;
    int vw = w;
    int vh = h;
    if (appearance.flags & Appearance::COVER6) {
      cw = appearance.coverWidth;
      ch = appearance.coverHeight;
      vw *= cw;
      vh *= ch;
    }
    int bx = (centerX >> DP_SHIFT) - vw / 2 + appearance.xOffset;
    int by = (centerY + c.h / 2 >> DP_SHIFT) - vh + appearance.yOffset;
    for (int y = 0; y < ch; ++y) {
      int bs = bx;
      for (int x = 0; x < cw; ++x) {
        SDL_Rect dst {
          .x = static_cast<Sint16>(bx),
          .y = static_cast<Sint16>(by),
          .w = static_cast<Uint16>(w),
          .h = static_cast<Uint16>(h),
        };
        SDL_Rect src {
          .x = static_cast<Sint16>(fx * fw),
          .y = static_cast<Sint16>(fy * fh),
          .w = static_cast<Uint16>(w),
          .h = static_cast<Uint16>(h),
        };
        SDL_BlitSurface(surface, &src, screen, &dst);
        bx += w;
        fx = x >= cw - 2 ? 2 : 1;
      }
      bx = bs;
      by += h;
      if (!y) ++fy;
      fx = 0;
    }
  }
}

void DinoJump::drawGround() {
  backgroundOffset %= ground->w << DP_SHIFT;
  if (backgroundOffset < 0) backgroundOffset += ground->w << DP_SHIFT;
  int pixelOffset = backgroundOffset >> DP_SHIFT;
  if (pixelOffset < screen->w) {
    SDL_Rect dstRect { .x = static_cast<Sint16>(pixelOffset), .y = static_cast<Sint16>(screen->h - ground->h) };
    SDL_BlitSurface(ground, nullptr, screen, &dstRect);
  }
  if (pixelOffset > 0) {
    SDL_Rect dstRect { .x = static_cast<Sint16>(pixelOffset-ground->w), .y = static_cast<Sint16>(screen->h - ground->h) };
    SDL_BlitSurface(ground, nullptr, screen, &dstRect);
  }
}

void DinoJump::render() {
  SDL_BlitSurface(bg, nullptr, screen, nullptr);
  drawGround();
  for (int i = 0; i < numObstacles; ++i) {
    const Obstacle *o = obstacles + i;
    drawCollider(o->collider, o->appearance);
  }
  drawCollider(dino.collider, dino.appearance);
  overlay.drawOverlay(screen);
#if BLOWUP
  SDL_LockSurface(realScreen);
  SDL_LockSurface(screen);
  uint32_t *sp = static_cast<uint32_t*>(screen->pixels);
  int32_t spp = screen->pitch >> 2;
  uint32_t *tp = static_cast<uint32_t*>(realScreen->pixels);
  int32_t tpp = realScreen->pitch >> 2;

#ifdef FLIP
  uint32_t sw = screen->w;
  uint32_t sh = screen->h;
#endif

  for (int y = 0; y < realScreen->h; ++y) {
    uint32_t *target = tp;
#ifdef FLIP
    uint32_t *source = sp + (sh - (y >> BLOWUP) - 1) * spp;
#else
    uint32_t *source = sp + (y >> BLOWUP) * spp;
#endif
    for (int x = 0; x < realScreen->w; ++x) {
#ifdef FLIP
      *target++ = source[sw - (x >> BLOWUP) - 1];
#else
      *target++ = source[x >> BLOWUP];
#endif
    }
    tp += tpp;
  }
  SDL_UnlockSurface(realScreen);
  SDL_UnlockSurface(screen);
  //SDL_Flip(realScreen);
  SDL_UpdateRect(realScreen, 0, 0, 0, 0);
#else
  SDL_Flip(screen);
#endif
}

DinoJump app;

void mainLoop() {
  app.loop();
}

#ifndef TEST
int main() {
  app.init();

#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop(mainLoop, 0, 1);
#endif

#ifndef __EMSCRIPTEN__
  app.run();
#endif
  return 0;
}
#endif
