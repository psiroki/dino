/*
  Based on fragments of the DraStic emulator source.
  The original copyright notice can be found below.
*/
/*
  Special customized version for the DraStic emulator that runs on
  Miyoo Mini (Plus), TRIMUI-SMART and Miyoo A30 handhelds.

  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>
  Copyright (C) 2022-2024 Steward Fu <steward.fu@gmail.com>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#if defined(MIYOO_AUDIO)
#include "miyoo_audio.hh"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/stat.h>


#define MI_AUDIO_SAMPLE_PER_FRAME 768

#include "mi_sys.h"
#include "mi_common_datatype.h"
#include "mi_ao.h"

static MI_AUDIO_Attr_t stSetAttr;
static MI_AUDIO_Attr_t stGetAttr;
static MI_AO_CHN AoChn = 0;
static MI_AUDIO_DEV AoDevId = 0;

class MiyooAudioDevice {
  Uint8 *mixbuf;
  Uint32 mixlen;
  SDL_AudioSpec spec;
public:
  MiyooAudioDevice(): mixbuf(nullptr) {
    spec.freq = 44100;
    spec.format = AUDIO_S16;
    spec.channels = 2;
    spec.samples = 2048;
    spec.userdata = this;
    spec.callback = nullptr;
  }

  inline void setSpec(const SDL_AudioSpec &newSpec) {
    memcpy(&spec, &newSpec, sizeof(spec));
  }

  inline SDL_AudioSpec& getSpec() {
    return spec;
  }

  inline Uint8* getBuf() {
    return mixbuf;
  }

  inline uint32_t getBufSize() {
    return mixlen;
  }

  int open();
  void close();
  void play();
};

int MiyooAudioDevice::open() {
  MI_S32 miret = 0;
  MI_S32 s32SetVolumeDb = 0;
  MI_S32 s32GetVolumeDb = 0;
  MI_SYS_ChnPort_t stAoChn0OutputPort0;

  int optimizedSamples = 256;  // don't care what the spec says
  mixlen = optimizedSamples * 2 * spec.channels;
  mixbuf = (Uint8 *) SDL_malloc(mixlen);
  if (!mixbuf) return -1;

  stSetAttr.eBitwidth = E_MI_AUDIO_BIT_WIDTH_16;
  stSetAttr.eWorkmode = E_MI_AUDIO_MODE_I2S_MASTER;
  stSetAttr.u32FrmNum = 6;
  stSetAttr.u32PtNumPerFrm = optimizedSamples;
  stSetAttr.u32ChnCnt = spec.channels;
  stSetAttr.eSoundmode = spec.channels == 2 ? E_MI_AUDIO_SOUND_MODE_STEREO : E_MI_AUDIO_SOUND_MODE_MONO;
  stSetAttr.eSamplerate = (MI_AUDIO_SampleRate_e)spec.freq;
  printf("%s, freq:%d sample:%d channels:%d\n", __func__, spec.freq, optimizedSamples, spec.channels);
  miret = MI_AO_SetPubAttr(AoDevId, &stSetAttr);
  if (miret != MI_SUCCESS) {
    printf("%s, failed to set PubAttr\n", __func__);
    return -1;
  }
  miret = MI_AO_GetPubAttr(AoDevId, &stGetAttr);
  if (miret != MI_SUCCESS) {
    printf("%s, failed to get PubAttr\n", __func__);
    return -1;
  }
  miret = MI_AO_Enable(AoDevId);
  if (miret != MI_SUCCESS) {
    printf("%s, failed to enable AO\n", __func__);
    return -1;
  }
  miret = MI_AO_EnableChn(AoDevId, AoChn);
  if (miret != MI_SUCCESS) {
    printf("%s, failed to enable Channel\n", __func__);
    return -1;
  }
  miret = MI_AO_SetVolume(AoDevId, s32SetVolumeDb);
  if (miret != MI_SUCCESS) {
    printf("%s, failed to set Volume\n", __func__);
    return -1;
  }
  MI_AO_GetVolume(AoDevId, &s32GetVolumeDb);
  stAoChn0OutputPort0.eModId = E_MI_MODULE_ID_AO;
  stAoChn0OutputPort0.u32DevId = AoDevId;
  stAoChn0OutputPort0.u32ChnId = AoChn;
  stAoChn0OutputPort0.u32PortId = 0;
  MI_SYS_SetChnOutputPortDepth(&stAoChn0OutputPort0, 12, 13);
  return 0;
}

void MiyooAudioDevice::close() {
  SDL_free(mixbuf);
  MI_AO_DisableChn(AoDevId, AoChn);
  MI_AO_Disable(AoDevId);
}

void MiyooAudioDevice::play() {
  MI_AUDIO_Frame_t aoTestFrame;
  MI_S32 s32RetSendStatus = 0;

  aoTestFrame.eBitwidth = stGetAttr.eBitwidth;
  aoTestFrame.eSoundmode = stGetAttr.eSoundmode;
  aoTestFrame.u32Len = mixlen;
  aoTestFrame.apVirAddr[0] = mixbuf;
  aoTestFrame.apVirAddr[1] = nullptr;
  do {
    s32RetSendStatus = MI_AO_SendFrame(AoDevId, AoChn, &aoTestFrame, 1);
    usleep(((stSetAttr.u32PtNumPerFrm * 1000) / stSetAttr.eSamplerate - 3) * 1000);
  }
  while(s32RetSendStatus == MI_AO_ERR_NOBUF);
}

static MiyooAudioDevice dev;
static pthread_t audioThread;

static void* audioThreadFunc(void *ptr) {
  SDL_AudioSpec &spec(dev.getSpec());
  if (!spec.callback) return nullptr;
  while (true) {
    spec.callback(spec.userdata, dev.getBuf(), dev.getBufSize());
    dev.play();
  }
  return nullptr;
}

int initMiyooAudio(const SDL_AudioSpec &spec) {
  dev.setSpec(spec);
  int result = dev.open();
  if (result) return result;
  pthread_create(&audioThread, nullptr, audioThreadFunc, nullptr);
  return 0;
}

#endif
