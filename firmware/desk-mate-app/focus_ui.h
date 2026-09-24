#ifndef DESKMATE_FOCUS_UI_H
#define DESKMATE_FOCUS_UI_H
#include "lvgl.h"
#include "focus_model.h"
#include <stdio.h>
#include <stdint.h>

/* One view for LVGL 8 (board) and 9 (desktop). No BLE calls from rendering. */
typedef bool (*FocusSendMedia)(uint16_t usage);
typedef int (*FocusReadBattery)(void);
static FocusTimer focus_timer;
static FocusSendMedia focus_send_media;
static FocusReadBattery focus_read_battery;
static lv_obj_t *focus_pager, *focus_pages[2], *focus_arcs[2];
static lv_obj_t *focus_digits, *focus_caption, *focus_primary_text;
static lv_obj_t *focus_mac_title, *focus_mac_status, *focus_mac_hint;
static lv_obj_t *focus_mac_mode, *focus_mac_left, *focus_mac_right;
static lv_obj_t *focus_dots[2], *focus_timer_hint, *focus_battery;
static int focus_page = 0;
static bool focus_connected = false, focus_volume = false;
static uint32_t focus_feedback_until = 0, focus_dots_until = 0;
static uint32_t focus_next_battery_at = 0;
static uint32_t focus_last_seconds = UINT32_MAX;
static bool focus_last_running = false, focus_last_finished = false;
static const uint32_t FOCUS_INK = 0xF3EFE7, FOCUS_MUTED = 0x858883;
static const uint32_t FOCUS_AMBER = 0xE8B66D, FOCUS_BLUE = 0x8CBCE5;

static void focus_refresh(void);
static void focus_action(int action);

static lv_obj_t *focus_object(lv_obj_t *parent, int x, int y, int w, int h) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}
static lv_obj_t *focus_label(lv_obj_t *p, const char *s, int y,
                             const lv_font_t *font, uint32_t color) {
    lv_obj_t *o = lv_label_create(p);
    lv_label_set_text(o, s);
    lv_obj_set_style_text_font(o, font, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(o, 360); lv_obj_set_pos(o, 60, y);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}
static void focus_clicked(lv_event_t *e) {
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
    lv_obj_add_event_cb(b, focus_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)action);
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, s);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
    lv_obj_center(l);
    lv_obj_clear_flag(l, LV_OBJ_FLAG_CLICKABLE);
    return l;
}
static lv_obj_t *focus_arc(lv_obj_t *p, int diameter, int width, bool background) {
    lv_obj_t *a = lv_arc_create(p);
    lv_obj_remove_style_all(a);
    lv_obj_set_size(a, diameter, diameter);
    lv_obj_center(a);
    lv_arc_set_rotation(a, 270);
    lv_arc_set_bg_angles(a, 0, 360);
    lv_arc_set_range(a, 0, 1000);
    lv_arc_set_value(a, 1000);
    lv_obj_set_style_arc_width(a, width, LV_PART_MAIN);
    lv_obj_set_style_arc_width(a, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(a, lv_color_hex(0x24201A), LV_PART_MAIN);
    lv_obj_set_style_arc_color(a, lv_color_hex(FOCUS_AMBER), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(a, background ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(a, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(a, true, LV_PART_INDICATOR);
    lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    return a;
}
static void focus_go_page(int page, bool animate) {
    if (page < 0) page = 0;
    if (page > 1) page = 1;
    lv_obj_scroll_to_x(focus_pager, page * 480, animate ? LV_ANIM_ON : LV_ANIM_OFF);
    focus_dots_until = lv_tick_get() + 2000;
}
static void focus_scroll(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_SCROLL_END) {
        int x = lv_obj_get_scroll_x(focus_pager);
        focus_page = x >= 240 ? 1 : 0;
    }
    focus_dots_until = lv_tick_get() + 1600;
}
static void focus_set_connected(bool connected) {
    if (connected == focus_connected) return;
    focus_connected = connected;
    focus_feedback_until = 0;
    focus_refresh();
}
static void focus_action(int action) {
    const uint32_t now = lv_tick_get();
    if (action == 4) { focus_go_page(1 - focus_page, true); return; }
    if (action == 5) { /* hold KEY or tap the secondary control */
        if (focus_page == 0) focus_reset(&focus_timer);
        else focus_volume = !focus_volume;
        focus_feedback_until = 0;
    } else if (focus_page == 0) {
        if (action == 0) focus_toggle(&focus_timer, now);
        else if (action == -1 || action == 1) focus_adjust(&focus_timer, action * 5);
        else if (action == 6 && !focus_timer.running &&
                 focus_timer.remaining_ms == focus_timer.duration_ms) {
            int minutes = focus_timer.duration_ms / 60000;
            int next = minutes == 25 ? 5 : minutes == 5 ? 50 : 25;
            focus_adjust(&focus_timer, next - minutes);
        }
    } else if (action >= -1 && action <= 1) {
        uint16_t usage = action == 0 ? (focus_volume ? 0xE2 : 0xCD) :
            action < 0 ? (focus_volume ? 0xEA : 0xB6) : (focus_volume ? 0xE9 : 0xB5);
        bool sent = focus_connected && focus_send_media && focus_send_media(usage);
        const char *label = !sent ? "Not connected" : action == 0 ?
            (focus_volume ? "Mute sent" : "Play / pause sent") : action < 0 ?
            (focus_volume ? "Volume down" : "Previous track") :
            (focus_volume ? "Volume up" : "Next track");
        lv_label_set_text(focus_mac_status, label);
        focus_feedback_until = now + 1600;
    }
    focus_refresh();
}
static void focus_refresh(void) {
    uint32_t now = lv_tick_get();
    if (!focus_next_battery_at || (int32_t)(now - focus_next_battery_at) >= 0) {
        int percent = focus_read_battery ? focus_read_battery() : -1;
        char battery[8];
        if (percent >= 0 && percent <= 100) snprintf(battery, sizeof(battery), "%d%%", percent);
        else snprintf(battery, sizeof(battery), "--%%");
        lv_label_set_text(focus_battery, battery);
        focus_next_battery_at = now + 15000;
    }
    focus_update(&focus_timer, now);
    unsigned seconds = (focus_timer.remaining_ms + 999) / 1000;
    bool changed = seconds != focus_last_seconds || focus_last_running != focus_timer.running ||
                   focus_last_finished != focus_timer.finished;
    if (changed) {
        char digits[12]; snprintf(digits, sizeof(digits), "%02u:%02u", seconds / 60, seconds % 60);
        lv_label_set_text(focus_digits, digits);
        lv_label_set_text(focus_caption, focus_timer.finished ? "SESSION COMPLETE" :
            focus_timer.running ? "ONE THING AT A TIME" :
            focus_timer.remaining_ms != focus_timer.duration_ms ? "TAKE YOUR TIME" : "MAKE A LITTLE SPACE");
        lv_label_set_text(focus_primary_text, focus_timer.running ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);
        lv_obj_center(focus_primary_text);
        lv_label_set_text(focus_timer_hint, focus_timer.finished ? "Well done. Start again when ready." :
            focus_timer.running ? "Hold KEY to reset" :
            focus_timer.remaining_ms != focus_timer.duration_ms ? "Paused  /  hold KEY to reset" : "BOOT  -5 min     KEY  +5 min");
        focus_last_seconds = seconds; focus_last_running = focus_timer.running;
        focus_last_finished = focus_timer.finished;
        int progress = (int)((uint64_t)focus_timer.remaining_ms * 1000 / focus_timer.duration_ms);
        lv_arc_set_value(focus_arcs[0], progress);
        lv_arc_set_value(focus_arcs[1], progress);
        if (focus_timer.remaining_ms != focus_timer.duration_ms || focus_timer.running || focus_timer.finished)
            lv_obj_clear_flag(focus_arcs[1], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(focus_arcs[1], LV_OBJ_FLAG_HIDDEN);
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
static void focus_ui_build(lv_obj_t *screen, FocusSendMedia send_media,
                           FocusReadBattery read_battery) {
    focus_timer = focus_timer_init(); focus_send_media = send_media;
    focus_read_battery = read_battery; focus_next_battery_at = 0;
    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    focus_pager = focus_object(screen, 0, 0, 480, 480);
    lv_obj_add_flag(focus_pager, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ONE | LV_OBJ_FLAG_SCROLL_MOMENTUM | LV_OBJ_FLAG_SCROLL_ELASTIC);
    lv_obj_set_scroll_dir(focus_pager, LV_DIR_HOR);
    lv_obj_set_scroll_snap_x(focus_pager, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scrollbar_mode(focus_pager, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_anim_time(focus_pager, 280, 0);
    lv_obj_add_event_cb(focus_pager, focus_scroll, LV_EVENT_SCROLL, NULL);
    lv_obj_add_event_cb(focus_pager, focus_scroll, LV_EVENT_SCROLL_END, NULL);
    for (int i = 0; i < 2; ++i) {
        focus_pages[i] = focus_object(focus_pager, i * 480, 0, 480, 480);
        lv_obj_add_flag(focus_pages[i], LV_OBJ_FLAG_SNAPPABLE);
        lv_obj_set_style_bg_color(focus_pages[i], lv_color_hex(0), 0);
        lv_obj_set_style_bg_opa(focus_pages[i], LV_OPA_COVER, 0);
    }
    lv_obj_t *p = focus_pages[0];
    focus_arcs[0] = focus_arc(p, 346, 3, true);
    focus_label(p, "F O C U S", 100, &lv_font_montserrat_16, FOCUS_AMBER);
    focus_caption = focus_label(p, "", 142, &lv_font_montserrat_12, FOCUS_MUTED);
    focus_digits = focus_label(p, "25:00", 187, &lv_font_montserrat_48, FOCUS_INK);
    lv_obj_set_style_text_letter_space(focus_digits, 3, 0);
    focus_button(p, "-", 84, 271, 72, 72, FOCUS_MUTED, -1);
    focus_primary_text = focus_button(p, LV_SYMBOL_PLAY, 196, 263, 88, 88, FOCUS_AMBER, 0);
    focus_button(p, "+", 324, 271, 72, 72, FOCUS_MUTED, 1);
    focus_button(p, "5 / 25 / 50", 145, 364, 190, 44, FOCUS_MUTED, 6);
    focus_timer_hint = focus_label(p, "", 418, &lv_font_montserrat_12, FOCUS_MUTED);
    p = focus_pages[1];
    focus_arcs[1] = focus_arc(p, 462, 2, false);
    focus_label(p, "M A C  C O N T R O L", 89, &lv_font_montserrat_16, FOCUS_BLUE);
    focus_mac_title = focus_label(p, "Hello, Mac.", 154, &lv_font_montserrat_48, FOCUS_INK);
    focus_mac_status = focus_label(p, "", 222, &lv_font_montserrat_12, FOCUS_BLUE);
    focus_mac_left = focus_button(p, LV_SYMBOL_PREV, 74, 263, 84, 84, FOCUS_BLUE, -1);
    focus_button(p, LV_SYMBOL_PLAY " / " LV_SYMBOL_PAUSE, 181, 259, 118, 92, FOCUS_BLUE, 0);
    focus_mac_right = focus_button(p, LV_SYMBOL_NEXT, 322, 263, 84, 84, FOCUS_BLUE, 1);
    focus_mac_mode = focus_button(p, "MUSIC  /  switch", 105, 369, 270, 48, FOCUS_MUTED, 5);
    focus_mac_hint = focus_label(p, "", 428, &lv_font_montserrat_12, FOCUS_MUTED);
    for (int i = 0; i < 2; ++i) {
        focus_dots[i] = focus_object(screen, 226 + i * 20, 454, 7, 7);
        lv_obj_set_style_radius(focus_dots[i], 4, 0);
        lv_obj_set_style_bg_opa(focus_dots[i], LV_OPA_COVER, 0);
        lv_obj_clear_flag(focus_dots[i], LV_OBJ_FLAG_CLICKABLE);
    }
    focus_battery = focus_label(screen, "--%", 20, &lv_font_montserrat_16, FOCUS_MUTED);
    lv_obj_set_width(focus_battery, 64);
    lv_obj_set_x(focus_battery, 396);
    lv_obj_set_style_text_align(focus_battery, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_move_foreground(focus_battery);
    focus_dots_until = lv_tick_get() + 3000;
    lv_obj_update_layout(screen);
    focus_refresh();
    lv_timer_create(focus_tick, 100, NULL);
}
#endif
