#pragma once
#include "platform.hh"

SDL_Surface* loadImage(const char* filename);
SDL_Surface* loadImageFromMemory(const void* contents, int size);
