#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../desk-mate-app/src/bili_voice/audio_convert.h"
#include "../desk-mate-app/src/bili_voice/pcm_ring.h"

static void ring_test(void) {
    static pcm_ring_t ring;
    pcm_ring_reset(&ring);
    uint8_t frame[VOICE_FRAME_BYTES];
    memset(frame, 0x5a, sizeof(frame));
    for (int i = 0; i < VOICE_RING_FRAMES; ++i) {
        frame[0] = (uint8_t)i;
        assert(pcm_ring_push(&ring, frame, VOICE_FRAME_BYTES));
    }
    assert(pcm_ring_used(&ring) == VOICE_RING_FRAMES);
    assert(!pcm_ring_push(&ring, frame, VOICE_FRAME_BYTES));
    assert(pcm_ring_dropped(&ring) == 1);
    uint8_t out[VOICE_FRAME_BYTES];
    uint16_t len = 0;
    for (int i = 0; i < VOICE_RING_FRAMES; ++i) {
        assert(pcm_ring_pop(&ring, out, &len));
        assert(len == VOICE_FRAME_BYTES && out[0] == (uint8_t)i);
    }
    assert(!pcm_ring_pop(&ring, out, &len));
    assert(pcm_ring_used(&ring) == 0);
    puts("ring: configured-frame bound, order, full rejection PASS");
}

static void convert_test(void) {
    int16_t stereo[6] = {1, -1, 2, -2, 32767, -32768};
    int16_t mono[3];
    audio_select_mono_s16(stereo, 3, 2, 0, mono);
    assert(mono[0] == 1 && mono[1] == 2 && mono[2] == 32767);
    audio_select_mono_s16(stereo, 3, 2, 1, mono);
    assert(mono[0] == -1 && mono[1] == -2 && mono[2] == -32768);
    int32_t pcm32[6] = {0x10000, 0, 0x7fffffff, 0, -0x10000, 0};
    size_t n = audio_pcm32_to_s16_mono(pcm32, 3, 2, 0, mono);
    assert(n == 3);
    assert(mono[0] == 1 && mono[1] == 32767 && mono[2] == -1);
    float rms = 0, peak = 0;
    int16_t tone[16];
    for (int i = 0; i < 16; ++i) tone[i] = (int16_t)(sin(i * 0.4) * 32767);
    audio_levels_s16(tone, 16, &rms, &peak);
    assert(rms > 0.2f && rms < 0.8f);
    assert(peak > 0.9f);
    puts("convert: channel select, 32->16 saturation, levels PASS");
}

int main(void) {
    ring_test();
    convert_test();
    puts("audio: all tests PASS");
    return 0;
}
