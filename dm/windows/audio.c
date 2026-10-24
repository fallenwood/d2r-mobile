// 基于 WinMM 的事件式双 / 单声道播放；输出失败明确记录并按真实时间消耗，不阻塞客体队列。
// Event-driven mono / stereo WinMM playback; output failures are logged and consumed in real time, never blocking the guest queue.
#include "audio.h"
#include <windows.h>
#include <mmsystem.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
enum { BLOCKS = 3, FRAMES = 480 };
struct dm_waveout {
    HWAVEOUT output;
    HANDLE event;
    CRITICAL_SECTION lock;
    pthread_t thread;
    WAVEHDR header[BLOCKS];
    int16_t data[BLOCKS][FRAMES * 2];
    dm_audio_render render;
    void *context;
    int running, playing, failed;
    unsigned channels;
};
static void *audio_main(void *argument) {
    dm_audev device = argument;
    ULONGLONG next_frame = 0;
    for (;;) {
        WaitForSingleObject(device->event, 10);
        EnterCriticalSection(&device->lock);
        if (!device->running) { LeaveCriticalSection(&device->lock); break; }
        if (device->playing) {
            if (device->failed) {
                ULONGLONG now = GetTickCount64();
                if (now >= next_frame) {
                    device->render(device->context, device->data[0], FRAMES * device->channels * 2);
                    next_frame = now + 10;
                }
            }
            else for (unsigned i = 0; i < BLOCKS; i++) if (!(device->header[i].dwFlags & WHDR_INQUEUE)) {
                device->render(device->context, device->data[i], device->header[i].dwBufferLength);
                next_frame = GetTickCount64() + 10;
                MMRESULT result = waveOutWrite(device->output, &device->header[i], sizeof device->header[i]);
                if (result) {
                    fprintf(stderr, "[dm] audio: waveOutWrite failed (%u); using a real-time silent sink\n", result);
                    device->failed = 1; waveOutReset(device->output); break;
                }
            }
        }
        LeaveCriticalSection(&device->lock);
    }
    return NULL;
}
dm_audev dm_waveout_open(unsigned channels, dm_audio_render render, void *context) {
    if ((channels != 1 && channels != 2) || !render) { fprintf(stderr, "[dm] audio: invalid WinMM format\n"); return NULL; }
    dm_audev device = calloc(1, sizeof *device); if (!device) return NULL;
    device->channels = channels; device->render = render; device->context = context;
    InitializeCriticalSection(&device->lock);
    device->event = CreateEventW(NULL, FALSE, FALSE, NULL);
    WAVEFORMATEX format = {.wFormatTag = WAVE_FORMAT_PCM, .nChannels = (WORD)channels, .nSamplesPerSec = 48000,
        .nAvgBytesPerSec = 48000 * channels * 2, .nBlockAlign = (WORD)(channels * 2), .wBitsPerSample = 16};
    MMRESULT result = device->event ? waveOutOpen(&device->output, WAVE_MAPPER, &format, (DWORD_PTR)device->event, 0, CALLBACK_EVENT) : MMSYSERR_NOMEM;
    if (result) { fprintf(stderr, "[dm] audio: waveOutOpen failed (%u)\n", result); goto fail; }
    for (unsigned i = 0; i < BLOCKS; i++) {
        device->header[i].lpData = (char *)device->data[i];
        device->header[i].dwBufferLength = FRAMES * channels * 2;
        result = waveOutPrepareHeader(device->output, &device->header[i], sizeof device->header[i]);
        if (result) { fprintf(stderr, "[dm] audio: waveOutPrepareHeader failed (%u)\n", result); goto fail; }
    }
    device->running = 1;
    int error = pthread_create(&device->thread, NULL, audio_main, device);
    if (error) { device->running = 0; fprintf(stderr, "[dm] audio: thread creation failed (%d)\n", error); goto fail; }
    return device;
fail:
    dm_waveout_close(device);
    return NULL;
}
int dm_waveout_start(dm_audev device) {
    EnterCriticalSection(&device->lock);
    MMRESULT result = waveOutRestart(device->output);
    if (result) fprintf(stderr, "[dm] audio: waveOutRestart failed (%u)\n", result);
    if (!result) device->playing = 1;
    LeaveCriticalSection(&device->lock);
    SetEvent(device->event);
    return result ? -1 : 0;
}
void dm_waveout_stop(dm_audev device) {
    EnterCriticalSection(&device->lock); device->playing = 0;
    MMRESULT result = waveOutReset(device->output);
    if (result) fprintf(stderr, "[dm] audio: waveOutReset failed (%u)\n", result);
    LeaveCriticalSection(&device->lock);
}
void dm_waveout_close(dm_audev device) {
    if (!device) return;
    int join = device->running;
    EnterCriticalSection(&device->lock); device->running = device->playing = 0; LeaveCriticalSection(&device->lock);
    if (device->event) SetEvent(device->event);
    if (join) pthread_join(device->thread, NULL);
    if (device->output) {
        waveOutReset(device->output);
        for (unsigned i = 0; i < BLOCKS; i++) if (device->header[i].dwFlags & WHDR_PREPARED)
            waveOutUnprepareHeader(device->output, &device->header[i], sizeof device->header[i]);
        waveOutClose(device->output);
    }
    if (device->event) CloseHandle(device->event);
    DeleteCriticalSection(&device->lock); free(device);
}
