#pragma once

#ifdef MIYOO_AUDIO
#include "platform.hh"

int initMiyooAudio(const SDL_AudioSpec &spec);
#endif
