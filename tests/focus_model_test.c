#include <assert.h>
#include <stdio.h>
#include "../firmware/desk-mate-app/focus_model.h"
int main(void) {
    FocusTimer t = focus_timer_init();
    assert(t.remaining_ms == 25u * 60000 && !t.running);
    focus_toggle(&t, 100);
    focus_update(&t, 1100);
    assert(t.remaining_ms == 1499000);
    focus_toggle(&t, 1600);
    focus_update(&t, 8000);
    assert(t.remaining_ms == 1498500 && !t.running);
    focus_toggle(&t, 10000);
    focus_update(&t, 10500);
    assert(t.remaining_ms == 1498000);
    focus_adjust(&t, 5);
    assert(t.duration_ms == 1500000); /* active sessions cannot change length */
    focus_reset(&t);
    focus_adjust(&t, -100);
    assert(t.duration_ms == 1000);
    focus_set_minutes(&t, 1);
    focus_adjust(&t, 1);
    assert(t.duration_ms == 120000); /* one click is always one minute */
    focus_adjust(&t, 200);
    assert(t.duration_ms == (99u * 60u + 59u) * 1000u);
    focus_set_minutes(&t, 15);
    assert(t.duration_ms == 15u * 60000 && t.remaining_ms == t.duration_ms);
    focus_adjust_seconds(&t, 1);
    assert(t.duration_ms == 15u * 60000 + 1000);
    focus_adjust_seconds(&t, -2);
    assert(t.duration_ms == 14u * 60000 + 59000);
    focus_set_minutes(&t, 1);
    focus_toggle(&t, 0xffffff00u);
    focus_update(&t, 744u);
    assert(t.remaining_ms == 59000);
    focus_update(&t, 70000u);
    assert(t.remaining_ms == 0 && t.finished && !t.running);
    focus_toggle(&t, 71000u);
    assert(t.running && !t.finished && t.remaining_ms == 60000);
    focus_reset(&t);
    assert(!t.running && !t.finished && t.remaining_ms == t.duration_ms);
    focus_toggle(&t, 72000);
    focus_update(&t, 73000);
    focus_restart(&t, 73000);
    assert(t.running && t.remaining_ms == t.duration_ms);
    puts("timer: pause/resume, expiry, bounds, rollover, reset PASS");
}
