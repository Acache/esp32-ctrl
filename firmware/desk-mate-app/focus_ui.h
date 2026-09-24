#ifndef DESKMATE_FOCUS_UI_H
#define DESKMATE_FOCUS_UI_H
#include "lvgl.h"
#include "focus_model.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

LV_FONT_DECLARE(focus_font_120);

typedef bool (*FocusSendMedia)(uint16_t usage);
typedef int (*FocusReadBattery)(void);
static FocusTimer focus_timer;
static FocusSendMedia focus_send_media;
static FocusReadBattery focus_read_battery;
static lv_obj_t *focus_pager, *focus_pages[2], *focus_arcs[2];
static lv_obj_t *focus_state, *focus_minute_digits, *focus_colon, *focus_second_digits;
static lv_obj_t *focus_caption, *focus_control_hint;
static lv_obj_t *focus_wheel_prev[2], *focus_wheel_next[2];
static lv_obj_t *focus_preset_overlay, *focus_preset_values[3];
static lv_obj_t *focus_mac_title, *focus_mac_status, *focus_mac_hint;
static lv_obj_t *focus_mac_mode, *focus_mac_left, *focus_mac_right;
static lv_obj_t *focus_dots[2], *focus_battery;
static int focus_page;
static bool focus_connected, focus_volume;
static uint32_t focus_feedback_until, focus_dots_until, focus_next_battery_at, focus_hint_until;
static uint32_t focus_last_seconds = UINT32_MAX;
static bool focus_last_running, focus_last_finished;
static const uint32_t FOCUS_INK = 0xF3EFE7, FOCUS_MUTED = 0x858883;
static const uint32_t FOCUS_AMBER = 0xE8B66D, FOCUS_BLUE = 0x8CBCE5;
static int focus_wheel_field = -1, focus_wheel_applied, focus_preset_index;
static lv_point_t focus_wheel_start;

static void focus_refresh(void);
static void focus_action(int action);
#if LVGL_VERSION_MAJOR >= 9
#define FOCUS_ACTIVE_INDEV() lv_indev_active()
#else
#define FOCUS_ACTIVE_INDEV() lv_indev_get_act()
#endif

static lv_obj_t *focus_object(lv_obj_t *parent, int x, int y, int w, int h) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o); lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE); return o;
}
static lv_obj_t *focus_label(lv_obj_t *p, const char *s, int y,
                             const lv_font_t *font, uint32_t color) {
    lv_obj_t *o = lv_label_create(p);
    lv_label_set_text(o, s); lv_obj_set_style_text_font(o, font, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(o, 360); lv_obj_set_pos(o, 60, y);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE); return o;
}
static void focus_button_event(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
        focus_action((int)(intptr_t)lv_event_get_user_data(e));
}
static lv_obj_t *focus_button(lv_obj_t *p, const char *s, int x, int y,
                              int w, int h, uint32_t color, int action) {
    lv_obj_t *b = focus_object(p, x, y, w, h);
    lv_obj_set_style_radius(b, h / 2, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x141719), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(0x2A2E30), LV_STATE_PRESSED);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(b, focus_button_event, LV_EVENT_CLICKED, (void *)(intptr_t)action);
    lv_obj_t *l = lv_label_create(b); lv_label_set_text(l, s);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_24, 0); lv_obj_center(l);
    lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE); return l;
}
static lv_obj_t *focus_arc(lv_obj_t *p, int x, int y, int d, int width, bool background) {
    lv_obj_t *a = lv_arc_create(p); lv_obj_remove_style_all(a);
    lv_obj_set_size(a, d, d); lv_obj_set_pos(a, x, y);
    lv_arc_set_rotation(a, 270); lv_arc_set_bg_angles(a, 0, 360);
    lv_arc_set_range(a, 0, 1000); lv_arc_set_value(a, 1000);
    lv_obj_set_style_arc_width(a, width, LV_PART_MAIN);
    lv_obj_set_style_arc_width(a, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(a, lv_color_hex(0x24201A), LV_PART_MAIN);
    lv_obj_set_style_arc_color(a, lv_color_hex(FOCUS_AMBER), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(a, background ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(a, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(a, true, LV_PART_INDICATOR);
    lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE); return a;
}
static void focus_go_page(int page, bool animate) {
    if (page < 0) page = 0; if (page > 1) page = 1;
    lv_obj_scroll_to_x(focus_pager, page * 480, animate ? LV_ANIM_ON : LV_ANIM_OFF);
    focus_dots_until = lv_tick_get() + 2000;
}
static void focus_scroll(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_SCROLL_END)
        focus_page = lv_obj_get_scroll_x(focus_pager) >= 240 ? 1 : 0;
    focus_dots_until = lv_tick_get() + 1600;
}
static void focus_set_connected(bool connected) {
    if (connected == focus_connected) return;
    focus_connected = connected; focus_feedback_until = 0; focus_refresh();
}
static void focus_anim_y(void *obj, int32_t y) { lv_obj_set_y((lv_obj_t *)obj, y); }
static void focus_snap_object(lv_obj_t *obj, int from_y, int to_y) {
    lv_anim_del(obj, focus_anim_y);
    lv_obj_set_y(obj, from_y);
    lv_anim_t animation; lv_anim_init(&animation);
    lv_anim_set_var(&animation, obj); lv_anim_set_exec_cb(&animation, focus_anim_y);
    lv_anim_set_values(&animation, from_y, to_y); lv_anim_set_time(&animation, 110);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out); lv_anim_start(&animation);
}
static void focus_wheel_snap(int field, int direction) {
    int offset = direction > 0 ? 12 : -12;
    focus_snap_object(focus_wheel_prev[field], 125 + offset, 125);
    focus_snap_object(field == 0 ? focus_minute_digits : focus_second_digits, 196 + offset, 196);
    focus_snap_object(focus_wheel_next[field], 295 + offset, 295);
}
static void focus_wheel_hide(void) {
    focus_wheel_field = -1;
    for (int i = 0; i < 2; ++i) {
        lv_obj_set_style_text_opa(focus_wheel_prev[i], LV_OPA_40, 0);
        lv_obj_set_style_text_opa(focus_wheel_next[i], LV_OPA_40, 0);
    }
}
static void focus_wheel_render(void) {
    unsigned total = (focus_timer.remaining_ms + 999) / 1000;
    char text[4];
    int values[2] = {(int)(total / 60), (int)(total % 60)};
    const int limits[2] = {100, 60};
    snprintf(text, sizeof(text), "%02d", values[0]); lv_label_set_text(focus_minute_digits, text);
    snprintf(text, sizeof(text), "%02d", values[1]); lv_label_set_text(focus_second_digits, text);
    for (int i = 0; i < 2; ++i) {
        snprintf(text, sizeof(text), "%02d", (values[i] + limits[i] - 1) % limits[i]);
        lv_label_set_text(focus_wheel_prev[i], text);
        snprintf(text, sizeof(text), "%02d", (values[i] + 1) % limits[i]);
        lv_label_set_text(focus_wheel_next[i], text);
        lv_opa_t opacity = focus_wheel_field == i ? LV_OPA_70 : LV_OPA_40;
        lv_obj_set_style_text_opa(focus_wheel_prev[i], opacity, 0);
        lv_obj_set_style_text_opa(focus_wheel_next[i], opacity, 0);
    }
}
static void focus_wheel_event(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    int field = (int)(intptr_t)lv_event_get_user_data(e);
    lv_indev_t *indev = FOCUS_ACTIVE_INDEV(); lv_point_t point = {0, 0};
    if (indev) lv_indev_get_point(indev, &point);
    if (code == LV_EVENT_PRESSED) {
        focus_wheel_field = field; focus_wheel_start = point; focus_wheel_applied = 0;
        focus_wheel_render();
    } else if (code == LV_EVENT_PRESSING) {
        int dx = point.x - focus_wheel_start.x, dy = point.y - focus_wheel_start.y;
        if (abs(dy) > 10 && abs(dy) > abs(dx) && !focus_timer.running &&
            focus_timer.remaining_ms == focus_timer.duration_ms) {
            int steps = -dy / 24, delta = steps - focus_wheel_applied;
            if (delta) {
                focus_adjust_seconds(&focus_timer, delta * (field == 0 ? 60 : 1));
                focus_wheel_applied = steps; focus_last_seconds = UINT32_MAX;
                focus_refresh(); focus_wheel_snap(field, delta);
            }
        }
    } else if (code == LV_EVENT_RELEASED) {
        int dx = point.x - focus_wheel_start.x, dy = point.y - focus_wheel_start.y;
        if (abs(dx) < 9 && abs(dy) < 9) focus_action(0);
        focus_wheel_hide();
    } else if (code == LV_EVENT_PRESS_LOST) focus_wheel_hide();
}
static void focus_preset_render(void) {
    for (int i = 0; i < 3; ++i) {
        bool selected = i == focus_preset_index;
        lv_obj_set_style_text_color(focus_preset_values[i],
            lv_color_hex(selected ? FOCUS_AMBER : FOCUS_MUTED), 0);
        lv_obj_set_style_text_opa(focus_preset_values[i], selected ? LV_OPA_COVER : LV_OPA_50, 0);
    }
}
static void focus_action(int action) {
    static const int presets[3] = {5, 15, 25};
    uint32_t now = lv_tick_get();
    if (action == 4) { focus_go_page(1 - focus_page, true); return; }
    if (action == 7) { focus_reset(&focus_timer); focus_last_seconds = UINT32_MAX; }
    else if (action == 8) { focus_restart(&focus_timer, now); focus_last_seconds = UINT32_MAX; }
    else if (action == 9) {
        unsigned m = focus_timer.duration_ms / 60000;
        focus_preset_index = m == 15 ? 1 : m == 25 ? 2 : 0;
        focus_preset_render(); lv_obj_clear_flag(focus_preset_overlay, LV_OBJ_FLAG_HIDDEN);
    } else if (action == 10) {
        focus_preset_index = (focus_preset_index + 1) % 3; focus_preset_render();
    } else if (action == 11) {
        focus_reset(&focus_timer); focus_set_minutes(&focus_timer, presets[focus_preset_index]);
        lv_obj_add_flag(focus_preset_overlay, LV_OBJ_FLAG_HIDDEN); focus_last_seconds = UINT32_MAX;
    } else if (action == 5) {
        if (focus_page == 1) focus_volume = !focus_volume;
        focus_feedback_until = 0;
    } else if (focus_page == 0) {
        if (action == 0) focus_toggle(&focus_timer, now);
    } else if (action >= -1 && action <= 1) {
        uint16_t usage = action == 0 ? (focus_volume ? 0xE2 : 0xCD) :
            action < 0 ? (focus_volume ? 0xEA : 0xB6) : (focus_volume ? 0xE9 : 0xB5);
        bool sent = focus_connected && focus_send_media && focus_send_media(usage);
        const char *label = !sent ? "Not connected" : action == 0 ?
            (focus_volume ? "Mute sent" : "Play / pause sent") : action < 0 ?
            (focus_volume ? "Volume down" : "Previous track") :
            (focus_volume ? "Volume up" : "Next track");
        lv_label_set_text(focus_mac_status, label); focus_feedback_until = now + 1600;
    }
    focus_refresh();
}
static void focus_refresh(void) {
    uint32_t now = lv_tick_get();
    if (!focus_next_battery_at || (int32_t)(now - focus_next_battery_at) >= 0) {
        int percent = focus_read_battery ? focus_read_battery() : -1; char battery[8];
        if (percent >= 0 && percent <= 100) snprintf(battery, sizeof(battery), "%d%%", percent);
        else snprintf(battery, sizeof(battery), "--%%");
        lv_label_set_text(focus_battery, battery); focus_next_battery_at = now + 15000;
    }
    focus_update(&focus_timer, now);
    unsigned seconds = (focus_timer.remaining_ms + 999) / 1000;
    bool changed = seconds != focus_last_seconds || focus_last_running != focus_timer.running ||
                   focus_last_finished != focus_timer.finished;
    if (changed) {
        focus_wheel_render();
        lv_label_set_text(focus_state, focus_timer.finished ? "REST" :
            focus_timer.running ? "FOCUSING" :
            focus_timer.remaining_ms != focus_timer.duration_ms ? "PAUSED" : "FOCUS");
        lv_label_set_text(focus_caption, focus_timer.finished ? "KEY TO RESET" :
            focus_timer.running ? "STAY WITH ONE THING" :
            focus_timer.remaining_ms != focus_timer.duration_ms ? "PWR TO CONTINUE" : "ROLL MINUTES  /  SECONDS");
        focus_last_seconds = seconds; focus_last_running = focus_timer.running;
        focus_last_finished = focus_timer.finished;
        int progress = focus_timer.duration_ms ?
            (int)((uint64_t)focus_timer.remaining_ms * 1000 / focus_timer.duration_ms) : 0;
        lv_arc_set_value(focus_arcs[0], progress); lv_arc_set_value(focus_arcs[1], progress);
        if (focus_timer.remaining_ms != focus_timer.duration_ms || focus_timer.running || focus_timer.finished)
            lv_obj_clear_flag(focus_arcs[1], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(focus_arcs[1], LV_OBJ_FLAG_HIDDEN);
    }
    if (focus_hint_until && (int32_t)(focus_hint_until - now) <= 0) {
        focus_hint_until = 0; lv_obj_set_style_text_opa(focus_control_hint, LV_OPA_TRANSP, 0);
    }
    lv_label_set_text(focus_mac_title, focus_connected ? "Ready." : "Hello, Mac.");
    lv_label_set_text(focus_mac_hint, focus_connected ? "Your music. Within reach." : "Pair Desk-Mate in Mac Bluetooth settings");
    if (!focus_feedback_until || (int32_t)(now - focus_feedback_until) >= 0) {
        focus_feedback_until = 0;
        lv_label_set_text(focus_mac_status, focus_connected ? "BLUETOOTH CONNECTED" : "WAITING FOR CONNECTION");
    }
    lv_label_set_text(focus_mac_mode, focus_volume ? "VOLUME  /  switch" : "MUSIC  /  switch");
    lv_label_set_text(focus_mac_left, focus_volume ? "-" : LV_SYMBOL_PREV);
    lv_label_set_text(focus_mac_right, focus_volume ? "+" : LV_SYMBOL_NEXT);
    lv_obj_center(focus_mac_left); lv_obj_center(focus_mac_right);
    for (int i = 0; i < 2; ++i) {
        lv_obj_set_style_bg_color(focus_dots[i], lv_color_hex(i == focus_page ? FOCUS_INK : 0x484B4D), 0);
        lv_obj_set_style_opa(focus_dots[i], (int32_t)(focus_dots_until - now) > 0 ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
}
static void focus_tick(lv_timer_t *t) { (void)t; focus_refresh(); }
static void focus_ui_build(lv_obj_t *screen, FocusSendMedia send_media, FocusReadBattery read_battery) {
    focus_timer = focus_timer_init(); focus_send_media = send_media; focus_read_battery = read_battery;
    focus_page = 0; focus_volume = false; focus_connected = false; focus_next_battery_at = 0;
    lv_obj_remove_style_all(screen); lv_obj_set_style_bg_color(screen, lv_color_hex(0), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0); lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    focus_pager = focus_object(screen, 0, 0, 480, 480);
    lv_obj_add_flag(focus_pager, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ONE |
        LV_OBJ_FLAG_SCROLL_MOMENTUM | LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_set_scroll_dir(focus_pager, LV_DIR_HOR); lv_obj_set_scroll_snap_x(focus_pager, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scrollbar_mode(focus_pager, LV_SCROLLBAR_MODE_OFF); lv_obj_set_style_anim_time(focus_pager, 280, 0);
    lv_obj_add_event_cb(focus_pager, focus_scroll, LV_EVENT_SCROLL, NULL);
    lv_obj_add_event_cb(focus_pager, focus_scroll, LV_EVENT_SCROLL_END, NULL);
    for (int i = 0; i < 2; ++i) {
        focus_pages[i] = focus_object(focus_pager, i * 480, 0, 480, 480);
        lv_obj_add_flag(focus_pages[i], LV_OBJ_FLAG_SNAPPABLE);
        lv_obj_set_style_bg_color(focus_pages[i], lv_color_hex(0), 0);
        lv_obj_set_style_bg_opa(focus_pages[i], LV_OPA_COVER, 0);
    }
    lv_obj_t *p = focus_pages[0];
    focus_arcs[0] = focus_arc(p, 32, 48, 416, 6, true);
    focus_state = focus_label(p, "FOCUS", 79, &lv_font_montserrat_24, FOCUS_AMBER);
    focus_minute_digits = focus_label(p, "25", 196, &focus_font_120, FOCUS_INK);
    lv_obj_set_width(focus_minute_digits, 164); lv_obj_set_x(focus_minute_digits, 55);
    focus_colon = focus_label(p, ":", 196, &focus_font_120, FOCUS_INK);
    lv_obj_set_width(focus_colon, 42); lv_obj_set_x(focus_colon, 219);
    focus_second_digits = focus_label(p, "00", 196, &focus_font_120, FOCUS_INK);
    lv_obj_set_width(focus_second_digits, 164); lv_obj_set_x(focus_second_digits, 261);
    focus_caption = focus_label(p, "", 348, &lv_font_montserrat_16, FOCUS_MUTED);
    for (int i = 0; i < 2; ++i) {
        int x = i == 0 ? 72 : 278;
        focus_wheel_prev[i] = focus_label(p, "00", 125, &lv_font_montserrat_48, FOCUS_MUTED);
        focus_wheel_next[i] = focus_label(p, "00", 295, &lv_font_montserrat_48, FOCUS_MUTED);
        lv_obj_set_width(focus_wheel_prev[i], 130); lv_obj_set_x(focus_wheel_prev[i], x);
        lv_obj_set_width(focus_wheel_next[i], 130); lv_obj_set_x(focus_wheel_next[i], x);
        lv_obj_set_style_text_opa(focus_wheel_prev[i], LV_OPA_40, 0);
        lv_obj_set_style_text_opa(focus_wheel_next[i], LV_OPA_40, 0);
    }
    focus_control_hint = focus_label(p, "PWR START  /  BOOT STOP  /  KEY RESET", 398, &lv_font_montserrat_14, FOCUS_MUTED);
    lv_obj_t *minute_zone = focus_object(p, 34, 105, 206, 283);
    lv_obj_t *second_zone = focus_object(p, 240, 105, 206, 283);
    lv_obj_add_flag(minute_zone, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_flag(second_zone, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(minute_zone, focus_wheel_event, LV_EVENT_ALL, (void *)(intptr_t)0);
    lv_obj_add_event_cb(second_zone, focus_wheel_event, LV_EVENT_ALL, (void *)(intptr_t)1);
    focus_preset_overlay = focus_object(p, 68, 116, 344, 252);
    lv_obj_set_style_radius(focus_preset_overlay, 32, 0); lv_obj_set_style_bg_color(focus_preset_overlay, lv_color_hex(0x090A0B), 0);
    lv_obj_set_style_bg_opa(focus_preset_overlay, LV_OPA_COVER, 0); lv_obj_set_style_border_width(focus_preset_overlay, 1, 0);
    lv_obj_set_style_border_color(focus_preset_overlay, lv_color_hex(0x2C2821), 0);
    lv_obj_t *title = focus_label(focus_preset_overlay, "RELEASE TO SET", 25, &lv_font_montserrat_14, FOCUS_MUTED);
    lv_obj_set_width(title, 300); lv_obj_set_x(title, 22);
    const char *values[3] = {"05", "15", "25"};
    for (int i = 0; i < 3; ++i) {
        focus_preset_values[i] = focus_label(focus_preset_overlay, values[i], 94, &lv_font_montserrat_48, FOCUS_MUTED);
        lv_obj_set_width(focus_preset_values[i], 92); lv_obj_set_x(focus_preset_values[i], 24 + i * 102);
    }
    lv_obj_t *preset_hint = focus_label(focus_preset_overlay, "KEEP HOLDING TO ADVANCE", 205, &lv_font_montserrat_14, FOCUS_MUTED);
    lv_obj_set_width(preset_hint, 300); lv_obj_set_x(preset_hint, 22); lv_obj_add_flag(focus_preset_overlay, LV_OBJ_FLAG_HIDDEN);
    p = focus_pages[1];
    focus_arcs[1] = focus_arc(p, 9, 9, 462, 2, false);
    focus_label(p, "M A C  C O N T R O L", 86, &lv_font_montserrat_20, FOCUS_BLUE);
    focus_mac_title = focus_label(p, "Hello, Mac.", 154, &lv_font_montserrat_48, FOCUS_INK);
    focus_mac_status = focus_label(p, "", 222, &lv_font_montserrat_14, FOCUS_BLUE);
    focus_mac_left = focus_button(p, LV_SYMBOL_PREV, 48, 251, 108, 108, FOCUS_BLUE, -1);
    focus_button(p, LV_SYMBOL_PLAY " / " LV_SYMBOL_PAUSE, 169, 245, 142, 120, FOCUS_BLUE, 0);
    focus_mac_right = focus_button(p, LV_SYMBOL_NEXT, 324, 251, 108, 108, FOCUS_BLUE, 1);
    focus_mac_mode = focus_button(p, "MUSIC  /  switch", 88, 382, 304, 62, FOCUS_MUTED, 5);
    focus_mac_hint = focus_label(p, "", 451, &lv_font_montserrat_14, FOCUS_MUTED);
    for (int i = 0; i < 2; ++i) {
        focus_dots[i] = focus_object(screen, 226 + i * 20, 24, 7, 7);
        lv_obj_set_style_radius(focus_dots[i], 4, 0); lv_obj_set_style_bg_opa(focus_dots[i], LV_OPA_COVER, 0);
        lv_obj_clear_flag(focus_dots[i], LV_OBJ_FLAG_CLICKABLE);
    }
    focus_battery = focus_label(screen, "--%", 20, &lv_font_montserrat_16, FOCUS_MUTED);
    lv_obj_set_width(focus_battery, 64); lv_obj_set_x(focus_battery, 396);
    lv_obj_set_style_text_align(focus_battery, LV_TEXT_ALIGN_RIGHT, 0); lv_obj_move_foreground(focus_battery);
    focus_dots_until = lv_tick_get() + 3000; focus_hint_until = lv_tick_get() + 5000;
    lv_obj_update_layout(screen); focus_refresh(); lv_timer_create(focus_tick, 100, NULL);
}
#endif
