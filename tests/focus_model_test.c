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
    assert(t.duration_ms == 60000);
    focus_adjust(&t, 200);
    assert(t.duration_ms == 90u * 60000);
    focus_adjust(&t, -89);
    focus_toggle(&t, 0xffffff00u);
    focus_update(&t, 744u);
    assert(t.remaining_ms == 59000);
    focus_update(&t, 70000u);
    assert(t.remaining_ms == 0 && t.finished && !t.running);
    focus_toggle(&t, 71000u);
    assert(t.running && !t.finished && t.remaining_ms == 60000);
    focus_reset(&t);
    assert(!t.running && !t.finished && t.remaining_ms == t.duration_ms);
    puts("timer: pause/resume, expiry, bounds, rollover, reset PASS");
}
