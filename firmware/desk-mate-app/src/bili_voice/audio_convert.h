#ifndef DESKMATE_AUDIO_CONVERT_H
#define DESKMATE_AUDIO_CONVERT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Select one validated channel from interleaved signed 16-bit PCM and
 * optionally downmix? This deliberately selects one channel, not a sum. */
void audio_select_mono_s16(const int16_t *src, size_t frames, uint8_t channels,
                           uint8_t channel, int16_t *dst);

/* Convert interleaved 32-bit signed slots to 16-bit with arithmetic shift,
 * saturation and de-interleaving; returns written samples. */
size_t audio_pcm32_to_s16_mono(const int32_t *src, size_t frames, uint8_t channels,
                               uint8_t channel, int16_t *dst);

/* Average all 32-bit slots into one mono s16 stream with saturation. */
size_t audio_pcm32_to_s16_mix(const int32_t *src, size_t frames, uint8_t channels,
                              int16_t *dst);

/* Convert vendor RMS/peak helpers. */
void audio_levels_s16(const int16_t *samples, size_t count, float *rms, float *peak);

#ifdef __cplusplus
}
#endif

#endif
