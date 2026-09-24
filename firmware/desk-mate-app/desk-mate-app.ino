#include <Arduino.h>
#include "lvgl.h"
#include "bsp_lvgl_port.h"
#include "./src/port_bsp/i2c_bsp.h"
#include "./src/port_bsp/axp2101_bsp.h"

#include "media_ble.h"
#include "focus_ui.h"

struct ButtonState;

I2cMasterBus I2cMasterBus_(GPIO_NUM_7, GPIO_NUM_8, I2C_NUM_0);

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Desk-Mate 2 / Focus + Mac");
  Custom_PmicPortInit(&I2cMasterBus_, 0x34);
  PMU_EnableButton();
  pinMode(9, INPUT_PULLUP);
  pinMode(10, INPUT_PULLUP);
  bsp_lvgl_init(I2cMasterBus_);
  if (bsp_lvgl_lock(0)) {
    focus_ui_build(lv_scr_act(), media_send, PMU_GetBatteryPercent);
    bsp_lvgl_unlock();
  }
  media_begin();
  Serial.printf("READY heap=%u\n", ESP.getFreeHeap());
}

struct ButtonState {
  uint8_t pin;
  bool raw = false, stable = false, held = false;
  uint32_t changed_at = 0, pressed_at = 0, repeat_at = 0;
  int active_short = 0, active_hold = 0;
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
      b.timer_key = focus_page == 0 && b.pin == 10;
      b.active_short = focus_page == 0 ? (b.pin == 9 ? 7 : 8) : short_action;
      b.active_hold = focus_page == 0 ? (b.pin == 9 ? 7 : 9) : hold_action;
    } else if (!b.held) {
      Serial.printf("BUTTON %u tap action=%d\n", b.pin, b.active_short);
      focus_action(b.active_short);
    } else if (b.timer_key) {
      Serial.println("BUTTON KEY preset commit");
      focus_action(11);
    }
  }
  if (b.stable && !b.held && now - b.pressed_at >= 700) {
    b.held = true;
    Serial.printf("BUTTON %u hold action=%d\n", b.pin, b.active_hold);
    focus_action(b.active_hold);
  } else if (b.stable && b.held && b.timer_key && (int32_t)(now - b.repeat_at) >= 0) {
    b.repeat_at = now + 700;
    focus_action(10);
  }
}

void loop() {
  static uint32_t last_pmu = 0, last_status = 0;
  if (bsp_lvgl_lock(50)) {
    media_poll();
    focus_set_connected(media_ready());
    poll_button(boot_button, -1, 4);
    poll_button(key_button, 1, 5);
    if (millis() - last_pmu >= 80) {
      last_pmu = millis();
      if (PMU_TakeShortPress()) { Serial.println("BUTTON PWR tap"); focus_action(0); }
    }
    if (millis() - last_status >= 10000) {
      last_status = millis();
      Serial.printf("STATUS page=%d scroll=%ld timer=%lu running=%d ble=%d heap=%u\n",
          focus_page, (long)lv_obj_get_scroll_x(focus_pager),
          (unsigned long)focus_timer.remaining_ms, focus_timer.running, media_ready(), ESP.getFreeHeap());
    }
    bsp_lvgl_unlock();
  }
  delay(5);
}
