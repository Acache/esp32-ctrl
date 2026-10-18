#include "pcm_ring.h"

#include <string.h>

void pcm_ring_reset(pcm_ring_t *ring) {
    if (!ring) return;
    ring->head = ring->tail = ring->count = 0;
    ring->dropped = 0;
}

bool pcm_ring_push(pcm_ring_t *ring, const uint8_t *frame, uint16_t len) {
    if (!ring || !frame || len == 0 || len > VOICE_FRAME_BYTES) return false;
    if (ring->count >= VOICE_RING_FRAMES) {
        ring->dropped++;
        return false;
    }
    uint16_t slot = ring->head;
    memcpy(&ring->data[(size_t)slot * VOICE_FRAME_BYTES], frame, len);
    ring->frame_len[slot] = len;
    ring->head = (uint16_t)((slot + 1) % VOICE_RING_FRAMES);
    ring->count++;
    return true;
}

bool pcm_ring_pop(pcm_ring_t *ring, uint8_t *out, uint16_t *len) {
    if (!ring || !out || !len || ring->count == 0) return false;
    uint16_t slot = ring->tail;
    uint16_t n = ring->frame_len[slot];
    memcpy(out, &ring->data[(size_t)slot * VOICE_FRAME_BYTES], n);
    *len = n;
    ring->tail = (uint16_t)((slot + 1) % VOICE_RING_FRAMES);
    ring->count--;
    return true;
}

uint16_t pcm_ring_used(const pcm_ring_t *ring) { return ring ? ring->count : 0; }
uint16_t pcm_ring_free(const pcm_ring_t *ring) {
    return ring ? (uint16_t)(VOICE_RING_FRAMES - ring->count) : 0;
}
uint32_t pcm_ring_dropped(const pcm_ring_t *ring) { return ring ? ring->dropped : 0; }
