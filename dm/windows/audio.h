// WinMM 输出接口：48 kHz PCM16，复用宿主的混音回调。
// WinMM output interface: 48 kHz PCM16, reusing the host mixing callback.
#pragma once
#include <stdint.h>
typedef struct dm_waveout *dm_audev;
typedef void (*dm_audio_render)(void *context, void *buffer, uint32_t bytes);
dm_audev dm_waveout_open(unsigned channels, dm_audio_render render, void *context);
int dm_waveout_start(dm_audev device);
void dm_waveout_stop(dm_audev device);
void dm_waveout_close(dm_audev device);
