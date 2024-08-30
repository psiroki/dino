#pragma once

#ifdef MIYOO_AUDIO
#include <SDL/SDL.h>

int initMiyooAudio(const SDL_AudioSpec &spec);
#endif
