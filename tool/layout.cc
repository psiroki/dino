#include <stdint.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <filesystem>

#include "../src/input.hh"

using namespace std;

std::string baseDir;

enum class Meaning {
  UP, DOWN, LEFT, RIGHT,
  NORTH, EAST, SOUTH, WEST,
  R1, L1, R2, L2,
  START, SELECT, MENU,
};

struct KeyDef {
  int code;
  const char *name;
  int meaningIndexOverride;
};

const Control mappingOrder[] = {
  // up, down, left, right,
  Control::UP, Control::DOWN, Control::LEFT, Control::RIGHT,
  // north, east, south, west,
  Control::NORTH, Control::EAST, Control::SOUTH, Control::WEST,
  // r1, l1, r2, l2,
  Control::R1, Control::L1, Control::R2, Control::L2,
  // start, select, menu
  Control::START, Control::SELECT, Control::MENU,
};

struct InputLayout {
  const char *layoutName;
  const KeyDef *keys;
  const int numKeys;
};

#define HAT(k) { static_cast<int>(k | TYPE_HAT), "Hat" #k, -1 }
#define BUTTON(k) { static_cast<int>(k | TYPE_BUTTON), "Button" #k, -1 }
#define KEY(k) { static_cast<int>(k), #k, -1 }
#define KEYO(k, o) { static_cast<int>(k), #k, static_cast<int>(o) }
#define LAYOUT(n, k) { .layoutName = n, .keys = k, .numKeys = sizeof(k) / sizeof(*k)  }

// up, down, left, right, north, east, south, west, r1, l1, r2, l2, start, select, menu
KeyDef pcLayout[] = { KEY(SDLK_UP), KEY(SDLK_DOWN), KEY(SDLK_LEFT), KEY(SDLK_RIGHT), KEY(SDLK_w), KEY(SDLK_d), KEY(SDLK_s), KEY(SDLK_a), KEY(SDLK_RSHIFT), KEY(SDLK_LSHIFT), KEY(SDLK_e), KEY(SDLK_q), KEY(SDLK_RETURN), KEY(SDLK_SPACE), KEY(SDLK_ESCAPE), };

// up, down, left, right,
// north, east, south, west,
// r1, l1, r2, l2,
// start, select, menu
KeyDef miyooLayout[] = {
  KEY(SDLK_UP), KEY(SDLK_DOWN), KEY(SDLK_LEFT), KEY(SDLK_RIGHT),
  KEY(SDLK_LSHIFT), KEY(SDLK_SPACE), KEY(SDLK_LCTRL), KEY(SDLK_LALT),
  KEY(SDLK_t), KEY(SDLK_e), KEY(SDLK_BACKSPACE), KEY(SDLK_TAB),
  KEY(SDLK_RETURN), KEY(SDLK_RCTRL), KEY(SDLK_ESCAPE),
};

// up, down, left, right,
// north, east, south, west,
// r1, l1, r2, l2,
// start, select, menu
KeyDef bittboyLayout[] = {  // PocketGo/Bittboy
  KEY(SDLK_UP), KEY(SDLK_DOWN), KEY(SDLK_LEFT), KEY(SDLK_RIGHT),
  KEY(SDLK_SPACE), KEY(SDLK_LCTRL), KEY(SDLK_LALT), KEY(SDLK_LSHIFT),
  KEY(SDLK_BACKSPACE), KEY(SDLK_TAB), KEY(SDLK_t), KEY(SDLK_e),
  KEY(SDLK_RETURN), KEY(SDLK_ESCAPE), KEY(SDLK_RCTRL),
};

// up, down, left, right, north, east, south, west,
// r1, l1, r2, l2, start, select, menu
KeyDef rg35xxLayout[] = {
  HAT(SDL_HAT_UP), HAT(SDL_HAT_DOWN), HAT(SDL_HAT_LEFT), HAT(SDL_HAT_RIGHT), BUTTON(6), BUTTON(3), BUTTON(4), BUTTON(5),
  BUTTON(8), BUTTON(7), BUTTON(14), BUTTON(13), BUTTON(10), BUTTON(9), BUTTON(11),
  // button 16 is the menu short release button or something
};

// RG35XX 2022 GarlicOS layout (the triggers are probably joystick axes, I didn't bother with them, mapped 10 and 4)
KeyDef rg35xx22Layout[] = {
  HAT(SDL_HAT_UP), HAT(SDL_HAT_DOWN), HAT(SDL_HAT_LEFT), HAT(SDL_HAT_RIGHT), BUTTON(2), BUTTON(0), BUTTON(1), BUTTON(3),
  BUTTON(6), BUTTON(5), BUTTON(10), BUTTON(4), BUTTON(8), BUTTON(7), BUTTON(9),
};

const InputLayout layouts[] = {
  LAYOUT("PC", pcLayout),
  LAYOUT("MiyooMini", miyooLayout),
  LAYOUT("Bittboy", bittboyLayout),
  LAYOUT("RG35XX", rg35xxLayout),
  LAYOUT("RG35XX22", rg35xx22Layout),
};
const int numLayouts = sizeof(layouts) / sizeof(*layouts);

void layoutKeys(const InputLayout &layout) {
  const int numKeys = layout.numKeys;
  bool taken[numKeys*2*16];
  int maxSize = numKeys;//sizeof(taken)/sizeof(*taken);
  int32_t bestProbe = INT32_MAX;
  int bestTableSize = 0;
  KeyHasher bestHasher;
  for (int tableSize = numKeys; tableSize <= maxSize; ++tableSize) {
    for (int64_t i = 0; i < numKeys*numKeys*1024*numKeys*32; ++i) {
      int64_t r = i >> 4;
      int64_t rp = r / (numKeys << 3);
      int64_t layer = rp / (numKeys << 3);
      int64_t row = rp % (numKeys << 3);
      int64_t col = r % (numKeys << 3);
      if (!row || !col || !layer) continue;
      int32_t m = static_cast<int32_t>(layer);
      int32_t n = static_cast<int32_t>(row);
      int32_t o = static_cast<int32_t>(col);
      int32_t s = static_cast<int32_t>(i & 15);
      memset(taken, 0, tableSize * sizeof(*taken));
      if (!(i & (16777215LL|(16777215LL << 16)))) {
        cerr << "Trying " << m << endl;
      }
      bool ok = true;
      int maxProbe = 0;
      KeyHasher hasher(m, n, o, s);
      for (int j = 0; j < numKeys; ++j) {
        int code = layout.keys[j].code;
        bool allTaken = true;
        int hash = hasher.hash(code);
        for (int k = 0; k < 4; ++k) {
          int index = (hash + k) % tableSize;
          if (!taken[index]) {
            allTaken = false;
            taken[index] = true;
            if (maxProbe <= k) {
              maxProbe = k+1;
            }
            break;
          }
        }
        if (allTaken) {
          ok = false;
          break;
        }
      }
      if (ok && maxProbe < bestProbe) {
        bestProbe = maxProbe;
        bestHasher = hasher;
        bestTableSize = tableSize;
        if (maxProbe == 1) break;
      }
    }
  }
  if (bestProbe < 16) {
    KeyHasher hasher = bestHasher;
    int tableSize = bestTableSize;
    cout << "\nLayout: " << layout.layoutName << endl;
    cout << "m: " << hasher.m << endl;
    cout << "n: " << hasher.n << endl;
    cout << "o: " << hasher.o << endl;
    cout << "s: " << hasher.s << endl;
    cout << "numKeys: " << numKeys << endl;
    cout << "maxProbe: " << bestProbe << endl;
    cout << "tableSize: " << bestTableSize << endl;
    for (int s = 0; s < bestTableSize; ++s) {
      cout << (taken[s] ? "O" : "_");
    }
    cout << endl;

    char buffer[256*4+32];
    const uint8_t *start = reinterpret_cast<const uint8_t*>(buffer);
    memset(buffer, 0, sizeof(buffer));
    KeyMapTable &hdr(*reinterpret_cast<KeyMapTable*>(buffer));
    int32_t *table = hdr.mappings;
    hdr.numEntries = tableSize;
    hdr.hasher = hasher;
    hdr.maxProbes = bestProbe;
    const uint8_t *end = reinterpret_cast<const uint8_t*>(hdr.mappings + tableSize*2);
    for (int j = 0; j < numKeys; ++j) {
      int code = layout.keys[j].code;
      int hash = hasher.hash(code);
      int base = (hash % tableSize)*2;
      table[base] = code;
      table[base + 1] = j;
    }
    for (int j = 0; j < numKeys; ++j) {
      if (table[j*2]) {
        int meaning = table[j*2+1];
        char c[2] = { static_cast<char>(meaning + 'A'), 0 };
        cout << c;
      } else {
        cout << "_";
      }
    }
    cout << endl;

    // map from meaning to actual game input
    for (int j = 0; j < numKeys; ++j) {
      if (table[j*2]) {
        int meaning = table[j*2+1];
        int override = layout.keys[meaning].meaningIndexOverride;
        if (override >= 0) meaning = override;
        table[j*2+1] = static_cast<int>(mappingOrder[meaning]);
      }
    }
    if (strncmp("PC", layout.layoutName, 3) == 0) {
      const uint8_t* ptr = start;
      int count = 0;
      std::cout << "const uint8_t defaultLayoutBytes[] = {";
      while (ptr < end) {
        if (!(count & 15)) {
          std::cout << std::endl << "  ";
        }
        std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(*ptr) << ",";
        ptr++;
        count++;
      }
      std::cout << std::dec << std::endl;
      std::cout << "};" << std::endl;
    }
    std::string filename = baseDir + "/../../assets/" + std::string(layout.layoutName) + ".layout";
    std::ofstream file(filename, std::ios::binary);
    if (file.is_open()) {
      file.write(reinterpret_cast<const char*>(start), end - start);
      file.close();
    } else {
      std::cerr << "Unable to open file: " << filename << std::endl;
    }
  }
}

void layoutAll() {
  std::cout << "Generating layouts..." << std::endl;
  for (int i = 0; i < numLayouts; ++i) {
    layoutKeys(layouts[i]);
  }
}


int main(int argc, const char **argv) {
  std::filesystem::path fsPath(argv[0]);
  baseDir = fsPath.parent_path().string();
  layoutAll();
}

