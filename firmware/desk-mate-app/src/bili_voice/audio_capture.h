#ifndef DESKMATE_VOICE_AUDIO_H
#define DESKMATE_VOICE_AUDIO_H

#include <Arduino.h>

#include "pcm_ring.h"

typedef void (*vc_audio_error_cb_t)(int code, const char *summary);

bool vc_audio_init(pcm_ring_t *ring, vc_audio_error_cb_t on_error);
bool vc_audio_start();
void vc_audio_stop();
bool vc_audio_active();
void vc_audio_reset();
float vc_audio_last_rms();
bool vc_audio_ready();

#endif
