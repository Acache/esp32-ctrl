#include "input_adapter.h"

#include <esp_timer.h>

static QueueHandle_t s_queue = nullptr;
static volatile bool s_healthy = true;
static volatile uint32_t s_dropped = 0;

static void post(voice_event_type_t type, voice_input_source_t source,
                 int64_t raw_us, int64_t stable_us) {
    if (!s_queue) return;
    voice_event_t ev = {};
    ev.type = type;
    ev.source = source;
    ev.raw_us = raw_us;
    ev.stable_us = stable_us;
    if (xQueueSend(s_queue, &ev, 0) != pdTRUE) {
        s_dropped++;
        s_healthy = false;
    }
}

struct DebouncedKey {
    uint8_t pin;
    voice_input_source_t source;
    bool raw;
    bool stable;
    int64_t changed_us;
    int64_t raw_edge_us;
};

static void sample(DebouncedKey &key) {
    int64_t now = esp_timer_get_time();
    bool down = digitalRead(key.pin) == LOW;
    if (down != key.raw) {
        key.raw = down;
        key.changed_us = now;
        key.raw_edge_us = now;
    }
    if (key.raw != key.stable && now - key.changed_us >= (int64_t)VOICE_DEBOUNCE_MS * 1000) {
        key.stable = key.raw;
        if (key.stable) {
            post(key.source == VOICE_INPUT_KEY10 ? VOICE_EV_A_DOWN : VOICE_EV_B_DOWN,
                 key.source, key.raw_edge_us, now);
        } else {
            post(key.source == VOICE_INPUT_KEY10 ? VOICE_EV_A_UP : VOICE_EV_B_UP,
                 key.source, key.raw_edge_us, now);
        }
    }
}

static void input_task(void *arg) {
    (void)arg;
    DebouncedKey a = {10, VOICE_INPUT_KEY10, false, false, 0, 0};
    DebouncedKey b = {9, VOICE_INPUT_BOOT9, false, false, 0, 0};
    // Seed stable state from the actual pins so a held key at boot does not
    // synthesize an edge (boot-with-key is handled by waiting for release).
    a.raw = a.stable = digitalRead(a.pin) == LOW;
    b.raw = b.stable = digitalRead(b.pin) == LOW;
    for (;;) {
        sample(a);
        sample(b);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void vc_input_begin(QueueHandle_t event_queue) {
    s_queue = event_queue;
    xTaskCreate(input_task, "voice_input", 1792, nullptr, 4, nullptr);
}

void vc_input_post_pmic_short() {
    int64_t now = esp_timer_get_time();
    post(VOICE_EV_C_SHORT, VOICE_INPUT_PMIC, now, now);
}

bool vc_input_queue_healthy() { return s_healthy; }
