#pragma once
#include <SDL/SDL.h>

SDL_Surface* loadPNG(const char* filename);
SDL_Surface* loadPNGFromMemory(const void* contents, int size);
