#ifndef DESKMATE_PCM_RING_H
#define DESKMATE_PCM_RING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "voice_contracts.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t data[VOICE_RING_BYTES];
    uint16_t frame_len[VOICE_RING_FRAMES];
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint16_t count;
    volatile uint32_t dropped;
} pcm_ring_t;

void pcm_ring_reset(pcm_ring_t *ring);
bool pcm_ring_push(pcm_ring_t *ring, const uint8_t *frame, uint16_t len);
/* Copies the next owned frame into out; returns false when empty. */
bool pcm_ring_pop(pcm_ring_t *ring, uint8_t *out, uint16_t *len);
uint16_t pcm_ring_used(const pcm_ring_t *ring);
uint16_t pcm_ring_free(const pcm_ring_t *ring);
uint32_t pcm_ring_dropped(const pcm_ring_t *ring);

#ifdef __cplusplus
}
#endif

#endif
