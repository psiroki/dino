#pragma once

#include "perftext.hh"

class Submenu;

enum class Command { nop, resume, quit, };

class GameSettings {
public:
  virtual int getDifficulty()=0;
  virtual void setDifficulty(int val)=0;
};

class Menu {
  PerfTextOverlay &overlay;
  GameSettings &settings;
  Submenu *main;
  Submenu *credits;
  Submenu *current;
public:
  Menu(PerfTextOverlay &overlay, GameSettings &settings);
  ~Menu();
  void reset();
  void moveVertical(int delta);
  void moveHorizontal(int delta);
  Command execute();
  void render();
};
