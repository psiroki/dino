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
#include "pack.hh"

#ifdef __EMSCRIPTEN__
// no blowup
#else
#ifdef DESKTOP
#define BLOWUP 2
#endif
#endif

#ifdef MIYOO
#define MIYOO_AUDIO
#include "miyoo_audio.hh"
#define BLOWUP 1
#define FLIP
#endif

#ifdef MIYOOA30
#define BLOWUP 1
#define VERTICAL
#endif

#ifdef RG35XX
#define BLOWUP 1
#endif

#if defined(DESKTOP)
static const char * const LAYOUT_FILE = "PC";
#elif defined(MIYOO) || defined(MIYOOA30)
static const char * const LAYOUT_FILE = "MiyooMini";
#elif defined(BITTBOY)
static const char * const LAYOUT_FILE = "Bittboy";
#elif defined(RG35XX22B)
#pragma message "Using layout RG35XX22B"
static const char * const LAYOUT_FILE = "RG35XX22B";
#elif defined(RG35XX22)
#pragma message "Using layout RG35XX22"
static const char * const LAYOUT_FILE = "RG35XX22";
#elif defined(RG35XX)
#pragma message "Using layout RG35XX"
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

template<typename T> class AutoDeleteArray {
  T* ptr;
public:
  AutoDeleteArray(T* ptr): ptr(ptr) { }
  ~AutoDeleteArray() {
    delete[] ptr;
    ptr = nullptr;
  }

  T* asPointer() {
    return ptr;
  }

  operator T*() {
    return ptr;
  }

  T* operator->() {
    return ptr;
  }
};

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
  Timestamp times[4];
  MixChannel soundsToAdd[soundQueueSize];
  int soundRead;
  int soundWrite;
  MixChannel channels[maxNumChannels];
  int numChannelsUsed;
  int currentTimes;
public:
  inline Mixer(): audioTime { 0, 0, 0, 0 }, currentTimes(0), soundRead(0), soundWrite(0) { }
  void audioCallback(uint8_t *stream, int len);
  void playSound(const SoundBuffer *buffer);
};

void Mixer::audioCallback(uint8_t *stream, int len) {
  uint64_t time = audioTime[currentTimes];
  int numSamples = len / 4;

  if (time < len) {
    std::cerr << "audioCallback at " << time << std::endl;
  }

  int nextWatch = (currentTimes + 1) & 3;
  times[nextWatch].reset();
  audioTime[nextWatch] = time + numSamples;
  currentTimes = nextWatch;

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
  int w = currentTimes;
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

  DynamicCollider shadowCollider;

  Dino() {
    collider.w = 24 << FP_SHIFT;
    collider.h = 24 << FP_SHIFT;
    resetPosition();

    shadowCollider = collider;
  }

  inline void duckEnabled(bool val) {
    collider.h = (val ? 12 : 24) << FP_SHIFT;
  }

  inline int duckHeight() {
    return 16 << FP_SHIFT;
  }

  inline void resetPosition() {
    collider.x = 0;
    collider.y = -collider.h / 2;
    collider.lastX = collider.x;
    collider.lastY = collider.y;
  }
};

struct Obstacle {
  static int idCounter;

  Collider collider;
  Appearance appearance;

  int id;

  Obstacle() {
    reset();
  }

  /// Resets to the given height, 0 means default
  void reset(int width = 0, int height=0);
};

int Obstacle::idCounter = 0;

void Obstacle::reset(int width, int height) {
  if (!height) height = 32 << FP_SHIFT;
  if (!width) width = 16 << FP_SHIFT;
  collider.w = width;
  collider.h = height;
  collider.x = (640 << FP_SHIFT) + collider.w / 2;
  collider.y = -collider.h / 2;
  id = ++idCounter;
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

  bool audioInitialized;
  SoundBuffer jump;
  SoundBuffer step;
  SoundBuffer collide;
  Mixer mixer;
  SDL_AudioSpec desiredAudioSpec;
  SDL_AudioSpec actualAudioSpec;

  bool running;

  int frame;
  int stepFrame;
  int lastStepFrame;
  int lastObstacleId;
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
  int score;
  Activity activity;
  Timestamp stopEnd;

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
  void initAudio();
  void initAssets();
  bool loadInputLayout(const char *fn);
  void audioCallback(uint8_t *stream, int len);
public:
  inline DinoJump():
      screen(nullptr),
      running(false),
      frame(0),
      audioInitialized(false),
      lastHatBits(0),
      activity(Activity::playing),
      random(micros()),
      cx(160 << FP_SHIFT),
      cy(320 << FP_SHIFT),
      jumpsLeft(JUMPS),
      duck(false),
      numObstacles(0),
      lastObstacleId(-1),
      difficulty(1),
      score(0),
      overlay(320, 240, 0) {
  }
  void init();
  void run();
  void loop();
};

void DinoJump::init() {
  if (screen) return;

  std::cerr << "1.." << std::endl;
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK);

  uint32_t flags = SDL_DOUBLEBUF | SDL_HWSURFACE;
  std::cerr << "2.." << std::endl;
#if BLOWUP
#ifdef VERTICAL
#ifdef MIYOOA30
  realScreen = SDL_SetVideoMode(240 << BLOWUP, 320 << BLOWUP, 32, SDL_HWSURFACE | SDL_FULLSCREEN | SDL_DOUBLEBUF);
  SDL_Flip(realScreen);
#endif
  realScreen = SDL_SetVideoMode(240 << BLOWUP, 320 << BLOWUP, 32, SDL_HWSURFACE | SDL_FULLSCREEN);
#else
  realScreen = SDL_SetVideoMode(320 << BLOWUP, 240 << BLOWUP, 32, 0);
#endif
  std::cerr << "2.1.." << std::endl;
  screen = SDL_CreateRGBSurface(0, 320, 240, 32,
      realScreen->format->Rmask, realScreen->format->Gmask, realScreen->format->Bmask,
      realScreen->format->Amask);
  std::cerr << "2.2.." << std::endl;
#else
#ifdef __EMSCRIPTEN__
  uint32_t additionalFlags = 0;
#else
  uint32_t additionalFlags = SDL_DOUBLEBUF | SDL_HWSURFACE;
#endif
  screen = SDL_SetVideoMode(320, 240, 32, flags | additionalFlags);
#endif


  std::cerr << "3.." << std::endl;
  memset(&controlState, 0, sizeof(controlState));
  if (LAYOUT_FILE) {
    char path[256];
    snprintf(path, sizeof(path), "assets/%s.layout", LAYOUT_FILE);
    if (!loadInputLayout(path)) {
      // try loading input.layout instead
      loadInputLayout("assets/input.layout");
    }
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

  collide.generateMono(8000, [](uint32_t index) -> int {
    if (index >= 2000 && index < 4000) return 0;
    int angle = index % 600;
    int32_t s = angle < 300 ? -16 : 16;
    int vol = 20000 - index * 2;
    return s * vol >> 4;
  });

  initAudio();

  if (SDL_NumJoysticks() > 0) {
    SDL_JoystickOpen(0);
  }
  std::cerr << "4.." << std::endl;
  SDL_WM_SetCaption("Dino Jump", nullptr);
  SDL_ShowCursor(false);
  initAssets();
}

bool DinoJump::loadInputLayout(const char *path) {
    std::cerr << "Using " << path << " for input layout" << std::endl;
    std::ifstream file(path, std::ios::binary);
    if (file.is_open()) {
      file.read(inputLayoutBytes, sizeof(inputLayoutBytes));

      if (file.gcount() >= sizeof(inputLayoutBytes)) {
          std::cerr << "Layout file is too big" << std::endl;
      }
      inputLayout.setTable(inputLayoutBytes);
      file.close();
      return true;
    } else {
      std::cerr << "Could not load layout file " << path << std::endl;
      return false;
    }
}

void DinoJump::initAssets() {
  std::ifstream packFile("assets/assets.bin", std::ifstream::binary | std::ifstream::ate);
  uint32_t size = packFile.tellg();
  packFile.seekg(0);
  AutoDeleteArray<char> buf(new char[size]);
  packFile.read(buf, size);
  packFile.close();
  SlicedBuffer *bin = reinterpret_cast<SlicedBuffer*>(buf.asPointer());
  dino.appearance.color = randomBrightColor();
  vita = bin->loadPNG("assets/vita.png");
  dino.appearance.surface = vita;
  dino.appearance.frameWidth = 24;
  dino.appearance.frameX = 4;
  dino.appearance.yOffset = 3;
  std::cerr << "5.." << std::endl;
  bg = bin->loadPNG("assets/sky.png");
  ground = bin->loadPNG("assets/ground.png");
  blimp = bin->loadPNG("assets/blimp.png");
  building = bin->loadPNG("assets/building.png");
  shadow = bin->loadPNG("assets/shadow.png");
  std::cerr << "6.." << std::endl;
  const int widening = 2;
  wideShadow = SDL_CreateRGBSurface(0, shadow->w * widening, shadow-> h, 32,
      shadow->format->Rmask, shadow->format->Gmask, shadow->format->Bmask,
      shadow->format->Amask);

  cy = (screen->h - ground->h + 1) << DP_SHIFT;

  std::cerr << "7.." << std::endl;
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


void DinoJump::initAudio() {
  if (audioInitialized) return;
  std::cerr << "Initializing audio" << std::endl;
  // sound doesn't seem to be working on miyoo
  desiredAudioSpec.freq = 44100;
  desiredAudioSpec.format = AUDIO_S16;
  desiredAudioSpec.channels = 2;
  desiredAudioSpec.samples = 2048;
  desiredAudioSpec.userdata = this;
  desiredAudioSpec.callback = callAudioCallback;
  memcpy(&actualAudioSpec, &desiredAudioSpec, sizeof(actualAudioSpec));
#ifdef MIYOO
  if (initMiyooAudio(desiredAudioSpec)) {
    std::cerr << "Failed to set up audio. Running without it." << std::endl;
    audioInitialized = true;
    return;
  }
#else
  char log[256] { 0 };
  SDL_AudioDriverName(log, sizeof(log));
  std::cerr << "Audio driver: " << log << std::endl;
  std::cerr << "Opening audio device" << std::endl;
  if (SDL_OpenAudio(&desiredAudioSpec, &actualAudioSpec)) {
    std::cerr << "Failed to set up audio. Running without it." << std::endl;
    audioInitialized = true;
    return;
  }
  std::cerr << "Freq: " << actualAudioSpec.freq << std::endl;
  std::cerr << "Format: " << actualAudioSpec.format << std::endl;
  std::cerr << "Channels: " << static_cast<int>(actualAudioSpec.channels) << std::endl;
  std::cerr << "Samples: " << actualAudioSpec.samples << std::endl;
  std::cerr << "Starting audio" << std::endl;
  SDL_PauseAudio(0);
#endif
  std::cerr << "Audio initialized" << std::endl;
  audioInitialized = true;
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
  std::cerr << "Entering main loop" << std::endl;

  while (running) {
    Timestamp frameStart;

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
    if (control == Control::R1 || control == Control::R2 ||
        (controlState[Control::SELECT] || controlState[Control::START]) && control == Control::RIGHT) {
      ++difficulty;
    }
    if ((control == Control::L1 || control == Control::L2 ||
        (controlState[Control::SELECT] || controlState[Control::START]) && control == Control::LEFT) &&
        difficulty > 1) {
      --difficulty;
    }
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
        mixer.playSound(&collide);
      } else if (c.overlaps(dino.shadowCollider) && obstacles[i].id != lastObstacleId) {
        lastObstacleId = obstacles[i].id;
        score += difficulty * (-dino.collider.y < (8 << FP_SHIFT) ? 12 : 6) / 3;
      }
    }

    stepFrame += (speed << FP_SHIFT) / 20;
    if (stepFrame >= (6 << FP_SHIFT)) {
      stepFrame %= (6 << FP_SHIFT);
    }
    int dinoFrame = (stepFrame >> FP_SHIFT) % 6;  // shouldn't go over 6 in theory, but just to be safe
    dino.appearance.frameX = (duck ? 17 : 4) + dinoFrame;
    if (lastStepFrame != dinoFrame && dinoFrame % 3 == 0 &&
        (dino.collider.y+dino.collider.h/2) > (-1 << FP_SHIFT)) {
      mixer.playSound(&step);
    }
    lastStepFrame = dinoFrame;
  } else if (activity == Activity::stopping) {
    dino.appearance.frameX = 13 + (frame % 6 >> 1);
    if (stopEnd.elapsedSeconds() > 0.0f) {
      activity = Activity::playing;
      dino.resetPosition();
      numObstacles = 0;
      score = 0;
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
  overlay.clear();
  char str[256];
  snprintf(str, sizeof(str), "Difficulty: %d", difficulty);
  overlay.write(overlay.getNumColumns() - strnlen(str, sizeof(str)) - 1, 1, str);
  snprintf(str, sizeof(str), "Score: %d", score);
  overlay.write(1, 1, str);
  overlay.drawOverlay(screen);
#if BLOWUP
  SDL_LockSurface(realScreen);
  SDL_LockSurface(screen);
  uint32_t *sp = static_cast<uint32_t*>(screen->pixels);
  int32_t spp = screen->pitch >> 2;
  uint32_t *tp = static_cast<uint32_t*>(realScreen->pixels);
  int32_t tpp = realScreen->pitch >> 2;

#ifdef VERTICAL
  for (int x = 0; x < realScreen->w; ++x) {
    uint32_t *source = sp + (x >> BLOWUP) * spp;
    uint32_t *target = tp + x + tpp * (realScreen->h - 1);
    for (int y = 0; y < realScreen->h; ++y) {
      *target = source[y >> BLOWUP] | 0xFF000000u;
      target -= tpp;
    }
  }
#else

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
      *target++ = source[sw - (x >> BLOWUP) - 1] | 0xFF000000u;
#else
      *target++ = source[x >> BLOWUP];
#endif
    }
    tp += tpp;
  }
#endif
  SDL_UnlockSurface(realScreen);
  SDL_UnlockSurface(screen);
  SDL_Flip(realScreen);
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
