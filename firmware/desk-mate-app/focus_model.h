#ifndef DESKMATE_FOCUS_MODEL_H
#define DESKMATE_FOCUS_MODEL_H
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t duration_ms, remaining_ms, last_ms;
    bool running, finished;
} FocusTimer;

static inline FocusTimer focus_timer_init(void) {
    FocusTimer t = {25u * 60000u, 25u * 60000u, 0, false, false};
    return t;
}
static inline void focus_update(FocusTimer *t, uint32_t now) {
    if (!t->running) return;
    uint32_t elapsed = now - t->last_ms; /* unsigned subtraction survives millis wrap */
    t->last_ms = now;
    if (elapsed >= t->remaining_ms) {
        t->remaining_ms = 0;
        t->running = false;
        t->finished = true;
    } else t->remaining_ms -= elapsed;
}
static inline void focus_toggle(FocusTimer *t, uint32_t now) {
    if (t->running) { focus_update(t, now); t->running = false; return; }
    if (t->finished) { t->remaining_ms = t->duration_ms; t->finished = false; }
    t->last_ms = now;
    t->running = true;
}
static inline void focus_reset(FocusTimer *t) {
    t->remaining_ms = t->duration_ms;
    t->running = t->finished = false;
}
static inline void focus_restart(FocusTimer *t, uint32_t now) {
    bool was_running = t->running;
    t->remaining_ms = t->duration_ms;
    t->finished = false;
    t->running = was_running;
    t->last_ms = now;
}
static inline void focus_adjust_seconds(FocusTimer *t, int seconds) {
    if (t->running || t->remaining_ms != t->duration_ms) return;
    int next = (int)(t->duration_ms / 1000) + seconds;
    if (next < 1) next = 1;
    if (next > 99 * 60 + 59) next = 99 * 60 + 59;
    t->duration_ms = t->remaining_ms = (uint32_t)next * 1000;
}
static inline void focus_adjust(FocusTimer *t, int minutes) {
    focus_adjust_seconds(t, minutes * 60);
}
static inline void focus_set_minutes(FocusTimer *t, int minutes) {
    if (t->running) return;
    if (minutes < 1) minutes = 1;
    if (minutes > 99) minutes = 99;
    t->duration_ms = t->remaining_ms = (uint32_t)minutes * 60000;
    t->finished = false;
}
#endif
