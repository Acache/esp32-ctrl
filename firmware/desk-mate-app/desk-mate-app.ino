#include <Arduino.h>
#include "lvgl.h"
#include "bsp_lvgl_port.h"
#include "./src/port_bsp/i2c_bsp.h"
#include "./src/port_bsp/axp2101_bsp.h"

#include "media_ble.h"
#include "focus_ui.h"
#include "./src/bili_voice/voice_controller.h"
#include "./src/bili_voice/provisioning.h"

struct ButtonState;

I2cMasterBus I2cMasterBus_(GPIO_NUM_7, GPIO_NUM_8, I2C_NUM_0);

static void read_bili_status(FocusBiliStatus *out) {
  if (!out) return;
  VoiceUiStatus st;
  vc_controller_read(&st);
  memset(out, 0, sizeof(*out));
  out->state = (int)st.state;
  out->result = (int)st.result;
  out->code = st.code;
  out->connected = st.connected ? 1 : 0;
  out->ready = st.ready ? 1 : 0;
  out->audio_ready = st.audio_ready ? 1 : 0;
  out->recording_ms = st.recording_ms;
  out->cancel_remaining_ms = st.cancel_remaining_ms;
  out->pcm_frames = st.pcm_frames;
  out->rms = st.rms;
  snprintf(out->summary, sizeof(out->summary), "%s", st.summary);
  snprintf(out->transport, sizeof(out->transport), "%s", st.transport);
  snprintf(out->command_id, sizeof(out->command_id), "%s", st.command_id);
}

static void send_feedback_bridge(const char *command_id, const char *rating,
                                 const char *dismissed_by, int touch_count, int countdown_ms) {
  vc_controller_feedback(command_id, rating, dismissed_by, touch_count, countdown_ms);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Desk-Mate 2 / Focus + Mac + Bilibili");
  Custom_PmicPortInit(&I2cMasterBus_, 0x34);
  PMU_EnableButton();
  pinMode(9, INPUT_PULLUP);
  pinMode(10, INPUT_PULLUP);
  /* Bring up the voice transport before LVGL/BLE so the Wi-Fi driver gets
   * contiguous internal RAM. The old pages are then initialized unchanged. */
  vc_controller_preinit_audio();
  vc_provisioning_begin();
  if (!vc_controller_begin()) {
    Serial.println("VOICE: controller init failed; old pages remain active");
  }
  bsp_lvgl_init(I2cMasterBus_);
  if (bsp_lvgl_lock(0)) {
    focus_ui_build(lv_scr_act(), media_send, PMU_GetBatteryPercent);
    focus_ui_set_bili_source(read_bili_status);
    focus_ui_set_feedback_sink(send_feedback_bridge);
    bsp_lvgl_unlock();
  }
  media_begin();
  vc_controller_set_page(0);
  Serial.printf("READY heap=%u\n", ESP.getFreeHeap());
}

struct ButtonState {
  uint8_t pin;
  bool raw = false, stable = false, held = false;
  uint32_t changed_at = 0, pressed_at = 0, repeat_at = 0;
  int active_short = 0, active_hold = 0;
  int owner_page = 0;
  bool suppressed = false;
  bool timer_key = false;
};
static ButtonState boot_button{9}, key_button{10};

static void poll_button(ButtonState &b, int short_action, int hold_action) {
  uint32_t now = millis();
  bool down = digitalRead(b.pin) == LOW;
  if (down != b.raw) { b.raw = down; b.changed_at = now; }
  if (b.raw != b.stable && now - b.changed_at >= 30) {
    b.stable = b.raw;
    if (b.stable) {
      b.pressed_at = now; b.repeat_at = now + 1400; b.held = false;
      b.owner_page = focus_page;
      b.suppressed = (focus_page == PAGE_BILI);
      b.timer_key = focus_page == PAGE_FOCUS && b.pin == 10;
      b.active_short = b.suppressed ? -100 : (focus_page == PAGE_FOCUS ? (b.pin == 9 ? 7 : 8) : short_action);
      b.active_hold = b.suppressed ? -100 : (focus_page == PAGE_FOCUS ? (b.pin == 9 ? 7 : 9) : hold_action);
    } else if (b.suppressed || b.owner_page != focus_page) {
      Serial.printf("BUTTON %u release suppressed (owner=%d page=%d)\n", b.pin, b.owner_page, focus_page);
    } else if (!b.held) {
      Serial.printf("BUTTON %u tap action=%d\n", b.pin, b.active_short);
      focus_action(b.active_short);
    } else if (b.timer_key) {
      Serial.println("BUTTON KEY preset commit");
      focus_action(11);
    }
    if (!b.stable) {
      b.suppressed = false;
      b.owner_page = focus_page;
      b.timer_key = false;
    }
  }
  if (b.stable && !b.held && now - b.pressed_at >= 700) {
    b.held = true;
    if (!b.suppressed && b.owner_page == focus_page) {
      Serial.printf("BUTTON %u hold action=%d\n", b.pin, b.active_hold);
      focus_action(b.active_hold);
    }
  } else if (b.stable && b.held && b.timer_key && !b.suppressed &&
             b.owner_page == focus_page && (int32_t)(now - b.repeat_at) >= 0) {
    b.repeat_at = now + 700;
    focus_action(10);
  }
}

void loop() {
  static uint32_t last_pmu = 0, last_status = 0;
  vc_controller_set_page(focus_page);
  if (bsp_lvgl_lock(50)) {
    media_poll();
    focus_set_connected(media_ready());
    poll_button(boot_button, -1, 4);
    poll_button(key_button, 1, 5);
    if (millis() - last_pmu >= 80) {
      last_pmu = millis();
      if (PMU_TakeShortPress()) {
        Serial.println("BUTTON PWR tap");
        if (focus_page == PAGE_BILI) {
          vc_controller_pmic_short();
          focus_go_page(PAGE_FOCUS, true);
        } else {
          focus_action(0);
        }
      }
    }
    if (millis() - last_status >= 10000) {
      last_status = millis();
      VoiceUiStatus vu;
      vc_controller_read(&vu);
      Serial.printf("STATUS page=%d scroll=%ld timer=%lu running=%d ble=%d heap=%u voice=%d result=%d net=%s\n",
          focus_page, (long)lv_obj_get_scroll_x(focus_pager),
          (unsigned long)focus_timer.remaining_ms, focus_timer.running, media_ready(), ESP.getFreeHeap(),
          (int)vu.state, (int)vu.result, vu.transport);
    }
    bsp_lvgl_unlock();
  }
  delay(5);
}
