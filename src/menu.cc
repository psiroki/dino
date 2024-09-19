#include "menu.hh"

#include <stdint.h>

namespace {

  namespace Flags {
    enum {
      passive,
      resume,
      credits,
      quit,
      mainMenu,
      difficulty,
    };
  }

  struct MenuItem {
    const char *caption;
    uint32_t flags;
  };

#define M(c, f) { .caption = c, .flags = f, }
#define P(c) M(c, Flags::passive)
#define EOM M(nullptr, 0)

  const MenuItem mainItems[] = {
    M("Resume", Flags::resume),
    M("Difficulty: %d", Flags::difficulty),
    M("Credits", Flags::credits),
    M("Quit", Flags::quit),
    EOM,
  };

  const MenuItem creditsItems[] = {
    P("Developed by Peter Siroki"),
    P(""),
    P("Art by Arks and"),
    P("  Mobile Game Graphics"),
    P(""),
    M("OK", Flags::mainMenu),
    EOM,
  };

}

class Submenu {
  const MenuItem * const items;
  int selection;
  int numMenuItems;
  int maxLength;

  void adjustSelection();
public:
  Submenu(const MenuItem *items);
  void reset();
  void moveVertical(int delta);
  inline const MenuItem* getSelection() {
    return items + selection;
  }
  void render(PerfTextOverlay &overlay, GameSettings &settings);
};

Submenu::Submenu(const MenuItem *items): items(items) {
  for (numMenuItems = 0; items[numMenuItems].caption; ++numMenuItems);
  maxLength = 0;
  for (int i = 0; i < numMenuItems; ++i) {
    int l = strnlen(items[i].caption, 256);
    if (l > maxLength) maxLength = l;
  }
  adjustSelection();
}


void Submenu::adjustSelection() {
  while (items[selection].flags == Flags::passive) {
    ++selection;
    if (selection >= numMenuItems) selection = 0;
  }
}

void Submenu::reset() {
  selection = 0;
  adjustSelection();
}

void Submenu::moveVertical(int delta) {
  selection += delta;
  selection %= numMenuItems;
  if (selection < 0) selection += numMenuItems;
  adjustSelection();
}

void Submenu::render(PerfTextOverlay &overlay, GameSettings &settings) {
  int row = (overlay.getNumRows() - numMenuItems) >> 1;
  int col = (overlay.getNumColumns() - maxLength - 2) >> 1;
  for (int i = 0; i < numMenuItems; ++i) {
    if (i == selection) overlay.write(col - 2, row + i, "* ");
    const char *s = items[i].caption;
    if (items[i].flags == Flags::difficulty) {
      char str[256];
      snprintf(str, sizeof(str), s, settings.getDifficulty());
      overlay.write(col, row + i, str);
    } else {
      if (*s) overlay.write(col, row + i, s);
    }
  }
}

Menu::Menu(PerfTextOverlay &overlay, GameSettings &settings): overlay(overlay), settings(settings) {
  main = new Submenu(mainItems);
  credits = new Submenu(creditsItems);
  current = main;
}

Menu::~Menu() {
  delete main;
  delete credits;
  main = credits = current = nullptr;
}

void Menu::reset() {
  current = main;
  current->reset();
}

void Menu::moveVertical(int delta) {
  current->moveVertical(delta);
}

void Menu::moveHorizontal(int delta) {
  const MenuItem *sel = current->getSelection();
  if (sel->flags == Flags::difficulty) {
    settings.setDifficulty(settings.getDifficulty() + delta);
  }
}

Command Menu::execute() {
  const MenuItem *sel = current->getSelection();
  switch (sel->flags) {
    case Flags::credits:
      current = credits;
      break;
    case Flags::mainMenu:
      current = main;
      break;
    case Flags::resume:
      return Command::resume;
    case Flags::quit:
      return Command::quit;
  }
  return Command::nop;
}

void Menu::render() {
  current->render(overlay, settings);
}
