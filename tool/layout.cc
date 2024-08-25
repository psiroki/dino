#include <stdint.h>
#include <iostream>
#include <fstream>
#include <memory>
#include <iomanip>
#include <string>
#include <filesystem>
#include <vector>
namespace fs = std::filesystem;

#include "../src/input.hh"
#include "../src/pack.hh"

using namespace std;

string baseDir;
string assets = "/assets/";

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
      bool ok = true;
      int maxProbe = 0;
      KeyHasher hasher(m, n, o, s);
      for (int j = 0; j < numKeys; ++j) {
        int code = layout.keys[j].code;
        bool allTaken = true;
        uint32_t hash = hasher.hash(code);
        for (int k = 0; k < 4; ++k) {
          uint32_t index = (hash + k) % tableSize;
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
      uint32_t hash = hasher.hash(code);
      uint32_t base = (hash % tableSize)*2;
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
      cout << "const uint8_t defaultLayoutBytes[] = {";
      while (ptr < end) {
        if (!(count & 15)) {
          cout << endl << "  ";
        }
        cout << "0x" << hex << setw(2) << setfill('0') << static_cast<int>(*ptr) << ",";
        ptr++;
        count++;
      }
      cout << dec << endl;
      cout << "};" << endl;
    }
    string filename = baseDir + "/../.." + assets + string(layout.layoutName) + ".layout";
    ofstream file(filename, ios::binary);
    if (file.is_open()) {
      file.write(reinterpret_cast<const char*>(start), end - start);
      file.close();
    } else {
      cerr << "Unable to open file: " << filename << endl;
    }
  }
}

void layoutAll() {
  cout << "Generating layouts..." << endl;
  for (int i = 0; i < numLayouts; ++i) {
    layoutKeys(layouts[i]);
  }
}

struct AssetFile {
  string name;
  string path;
  uint32_t size;
};

struct SlicedBufferEditor: public SlicedBuffer {
  inline SlicedBufferEditor() {
    setMagic();
  }

  inline void setMagic() {
    magic = MAGIC;
  }

  inline uint32_t* getTable() {
    return table();
  }

  inline BufferSlice* getSlices() {
    return slices();
  }

  inline uint32_t* getContents() {
    return contents();
  }
};

KeyHasher packFiles(vector<AssetFile> names) {
  uint32_t overallSize = 0;
  for (AssetFile file: names) overallSize += (file.size + 3) & ~3;
  const int numKeys = names.size();
  bool taken[numKeys*2*16];
  int maxSize = numKeys;
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
      bool ok = true;
      int maxProbe = 0;
      KeyHasher hasher(m, n, o, s);
      for (int j = 0; j < numKeys; ++j) {
        const char *code = names[j].name.c_str();
        bool allTaken = true;
        uint32_t hash = hasher.hash(code);
        for (int k = 0; k < 1; ++k) {
          uint32_t index = (hash + k) % tableSize;
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
    cout << "\nFilenames" << endl;
    cout << "m: " << hasher.m << endl;
    cout << "n: " << hasher.n << endl;
    cout << "o: " << hasher.o << endl;
    cout << "s: " << hasher.s << endl;
    cout << "numKeys: " << numKeys << endl;
    cout << "maxProbe: " << bestProbe << endl;
    cout << "tableSize: " << bestTableSize << endl;
    memset(taken, 0, tableSize * sizeof(*taken));
    for (int j = 0; j < numKeys; ++j) {
      const char *code = names[j].name.c_str();
      bool allTaken = true;
      uint32_t hash = hasher.hash(code);
      uint32_t index = hash % tableSize;
      taken[index] = true;
    }
    for (int s = 0; s < bestTableSize; ++s) {
      cout << (taken[s] ? "O" : "_");
    }
    cout << endl;

    uint32_t bufferWordSize = (sizeof(SlicedBuffer) + // file header
      tableSize * 2 * 4 + // hashtable entries
      sizeof(BufferSlice) * names.size() +  // slices
      overallSize)  // file content
      >> 2;
    unique_ptr<uint32_t[]> buffer = make_unique<uint32_t[]>(bufferWordSize);
    uint32_t *start = buffer.get();
    uint32_t *end = start + bufferWordSize;
    memset(start, 0, bufferWordSize*4);
    SlicedBufferEditor *sbe = reinterpret_cast<SlicedBufferEditor*>(start);
    sbe->setMagic();
    sbe->hasher = hasher;
    sbe->numSlices = names.size();
    sbe->numTableEntries = tableSize;
    sbe->maxProbes = bestProbe;
    sbe->flags = 0;
    uint32_t *table = sbe->getTable();
    BufferSlice *slices = sbe->getSlices();
    uint32_t *contentPos = sbe->getContents();
    for (int i = 0; i < tableSize; ++i) {
      table[i*2] = ~0U;
      table[i*2+1] = ~0U;
    }
    for (int j = 0; j < numKeys; ++j) {
      const char *code = names[j].name.c_str();
      uint32_t hash = hasher.hash(code);
      cout << "File name: " << code << ", hash: " << hash << endl;
      uint32_t index = (hash % tableSize)*2;
      table[index] = hash;
      table[index + 1] = j;
      uint32_t *contentEnd = contentPos + ((names[j].size + 3) >> 2);
      if (contentEnd > end) {
        cerr << "Assertion failed, end pointer is over end: " << contentEnd << " > " << end << endl;
        exit(1);
      }
      ifstream stream(names[j].path, ifstream::binary);
      stream.read(reinterpret_cast<char*>(contentPos), names[j].size);
      slices[j].set(contentPos, names[j].size);
      if (slices[j].ptr() != contentPos) {
        cerr << "Assertion failed, pointer did not resolve correctly: got " << slices[j].ptr() << " instead of " << contentPos << endl;
        exit(1);
      }
      contentPos = contentEnd;
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
    for (int i = 0; i < tableSize; ++i) {
      if (~table[i*2]) {
        uint32_t h = table[i*2];
        BufferSlice *slice = slices + table[i*2+1];
        uint32_t *p = reinterpret_cast<uint32_t*>(slice->ptr());
        uint32_t size = (slice->sizeInBytes + 3) >> 2;
        for (int j = 0; j < size; ++j) {
          uint32_t op = p[j];
          p[j] ^= h;
          h += hasher.hash(op);
        }
      }
    }
    sbe->flags |= 1;
    ofstream output(baseDir+"/../.."+assets+"assets.bin", ofstream::binary);
    if (output.is_open()) {
      output.write(reinterpret_cast<char*>(start), bufferWordSize << 2);
      output.close();
    }
  }
  return bestHasher;
}

void packFiles() {
  cout << "Packing files..." << endl;
  vector<AssetFile> files;
  for (const fs::directory_entry &entry: fs::directory_iterator(baseDir+"/../.."+assets)) {
    if (!entry.is_regular_file()) continue;
    fs::path path = entry.path();
    string ext = path.extension().string();
    if (ext == ".layout" || ext == ".bin") continue;
    string ps = path.string();
    uint32_t offset = ps.find(assets);
    if (offset == string::npos) {
      cerr << ps << " seems fishy" << endl;
    }
    ++offset;
    ifstream file(path.string(), ifstream::ate | ifstream::binary);
    uint32_t fileSize = file.tellg();
    AssetFile assetFile { .name = ps.substr(offset), .path = path.string(), .size = fileSize };
    files.push_back(assetFile);
  }
  KeyHasher hasher = packFiles(files);

}

int main(int argc, const char **argv) {
  fs::path fsPath(argv[0]);
  baseDir = fsPath.parent_path().string();
  string arg1 = argv[1] ? string(argv[1]) : "";
  if (!arg1.length() || arg1 == "layouts") layoutAll();
  if (!arg1.length() || arg1 == "pack") packFiles();
  return 0;
}

