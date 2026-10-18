#include "audio_convert.h"

#include <math.h>

void audio_select_mono_s16(const int16_t *src, size_t frames, uint8_t channels,
                           uint8_t channel, int16_t *dst) {
    if (!src || !dst || channels == 0 || channel >= channels) return;
    for (size_t i = 0; i < frames; ++i) {
        dst[i] = src[i * channels + channel];
    }
}

size_t audio_pcm32_to_s16_mono(const int32_t *src, size_t frames, uint8_t channels,
                               uint8_t channel, int16_t *dst) {
    if (!src || !dst || channels == 0 || channel >= channels) return 0;
    for (size_t i = 0; i < frames; ++i) {
        int32_t v = src[i * channels + channel] >> 16;
        if (v > 32767) v = 32767;
        if (v < -32768) v = -32768;
        dst[i] = (int16_t)v;
    }
    return frames;
}

size_t audio_pcm32_to_s16_mix(const int32_t *src, size_t frames, uint8_t channels,
                              int16_t *dst) {
    if (!src || !dst || channels == 0) return 0;
    for (size_t i = 0; i < frames; ++i) {
        int64_t sum = 0;
        for (uint8_t c = 0; c < channels; ++c) sum += src[i * channels + c] >> 16;
        int64_t v = sum / channels;
        if (v > 32767) v = 32767;
        if (v < -32768) v = -32768;
        dst[i] = (int16_t)v;
    }
    return frames;
}

void audio_levels_s16(const int16_t *samples, size_t count, float *rms, float *peak) {
    if (!samples || count == 0) {
        if (rms) *rms = 0.0f;
        if (peak) *peak = 0.0f;
        return;
    }
    double sum = 0.0;
    int32_t maxv = 0;
    for (size_t i = 0; i < count; ++i) {
        int32_t v = samples[i];
        sum += (double)v * (double)v;
        int32_t a = v < 0 ? -v : v;
        if (a > maxv) maxv = a;
    }
    if (rms) *rms = (float)sqrt(sum / (double)count) / 32768.0f;
    if (peak) *peak = (float)maxv / 32768.0f;
}
