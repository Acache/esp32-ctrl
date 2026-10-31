/**
 * ui_pages.h — Desk-Mate UI 源码（模拟器 v9 + 硬件 v8 统一版本）
 */
#ifndef UI_PAGES_H
#define UI_PAGES_H

#ifdef DESKMATE_UI_IMPL

#include "jokes.h"
#include "einstein_img.h"

#ifndef LV_USE_SDL
#include "./src/port_bsp/axp2101_bsp.h"
#else
static int PMU_GetBatteryPercent(void) { return 85; }
#endif

#ifdef LV_USE_SDL
  #define IMG_CREATE(p)       lv_image_create(p)
  #define IMG_SET_SRC(o, s)   lv_image_set_src(o, s)
  #define IMG_SET_SCALE(o, v) lv_image_set_scale(o, v)
  #define TICK_GET()          lv_tick_get()
  #define RAND()              rand()
  #define TXT_GET_SIZE(s, t, f, ls, l, w, fl) lv_text_get_size(s, t, f, ls, l, w, fl)
  #define SET_STYLE_IMG_OPA(o, opa, sel)       lv_obj_set_style_image_opa(o, opa, sel)
  #define CLIP_CHILDREN(o)    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE)
#else
  #define IMG_CREATE(p)       lv_img_create(p)
  #define IMG_SET_SRC(o, s)   lv_img_set_src(o, s)
  #define IMG_SET_SCALE(o, v) lv_img_set_zoom(o, v)
  #define TICK_GET()          millis()
  #define RAND()              random()
  #define TXT_GET_SIZE(s, t, f, ls, l, w, fl) lv_txt_get_size(s, t, f, ls, l, w, fl)
  #define SET_STYLE_IMG_OPA(o, opa, sel)       lv_obj_set_style_img_opa(o, opa, sel)
  #define CLIP_CHILDREN(o)
#endif

#ifdef LV_USE_SDL
extern lv_font_t font_tnr_20;
extern lv_font_t font_cn_20;
extern lv_font_t font_cn_28;
#define F12_UI &lv_font_montserrat_16
#define F20_UI &font_tnr_20
#define F28_UI &font_cn_28
#else
#define F12_UI &lv_font_montserrat_16
#define F20_UI &lv_font_montserrat_20
#define F28_UI &lv_font_montserrat_24
#endif

#define CLR(r,g,b) lv_color_make(r,g,b)
#define BG_UI  CLR(0x0D,0x0D,0x1A)
#define SB_UI  CLR(0x0A,0x0A,0x14)
#define CY_UI  CLR(0x00,0xD4,0xFF)
#define GN_UI  CLR(0x00,0xE6,0x76)
#define WH_UI  CLR(0xEE,0xEE,0xEE)
#define BD_UI  CLR(0x22,0x22,0x3A)

static lv_obj_t *ui_pet_avatar = NULL;
static lv_obj_t *ui_pet_image = NULL;
static lv_obj_t *ui_joke_label = NULL;
static lv_obj_t *ui_battery_label = NULL;
static lv_obj_t *ui_time_label = NULL;
static bool ui_joke_visible = false;
static uint32_t ui_joke_hide_at = 0;

static uint32_t ui_boot_timestamp = 1780330795;
static uint32_t ui_boot_millis    = 0;
static int32_t  ui_tz_offset_sec  = 8 * 3600;

static lv_obj_t *ui_page_home_obj = NULL;
static lv_obj_t *ui_page_settings_obj = NULL;
static int ui_current_page = 0;

static lv_obj_t *ui_set_tz_label = NULL;
static lv_obj_t *ui_set_time_label = NULL;
static lv_obj_t *ui_time_popup = NULL;
static lv_obj_t *ui_popup_tz_label = NULL;
static lv_obj_t *ui_popup_time_label = NULL;

static lv_obj_t *ui_lb(lv_obj_t *p, const char *t, int x, int y, lv_color_t c, const lv_font_t *f) {
    lv_obj_t *l = lv_label_create(p);
    lv_label_set_text(l, t);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_style_text_color(l, c, 0);
    if (f) lv_obj_set_style_text_font(l, f, 0);
    return l;
}

static lv_obj_t *ui_build_sb(lv_obj_t *scr) {
    /* 状态栏：完全独立于所有 page，使用 LVGL 的顶层 layer，永不被 page 渲染覆盖 */
#ifdef LV_USE_SDL
    /* v9: lv_layer_top() 返回一个 layer obj */
    lv_obj_t *sb_parent = lv_layer_top();
#else
    /* v8: 同上 */
    lv_obj_t *sb_parent = lv_layer_top();
#endif
    lv_obj_t *sb = lv_obj_create(sb_parent);
    lv_obj_set_size(sb, 480, 28);
    lv_obj_set_pos(sb, 0, 0);
    lv_obj_set_style_bg_color(sb, SB_UI, 0);
    lv_obj_set_style_bg_opa(sb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sb, 0, 0);
    lv_obj_set_style_pad_all(sb, 0, 0);
    lv_obj_set_scrollbar_mode(sb, LV_SCROLLBAR_MODE_OFF);
    /* 禁用所有事件 */
    lv_obj_clear_flag(sb, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(sb, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(sb, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_clear_flag(sb, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_clear_flag(sb, LV_OBJ_FLAG_EVENT_BUBBLE);

    /* 状态栏内部子控件也都禁用 */
    lv_obj_t *lbl_name = ui_lb(sb, "Desk-Mate", 40, 3, CY_UI, F12_UI);
    lv_obj_clear_flag(lbl_name, LV_OBJ_FLAG_CLICKABLE);
    char bat_str[8];
    int pct = PMU_GetBatteryPercent();
    if (pct < 0) pct = 0;
    snprintf(bat_str, sizeof(bat_str), "%d%%", pct);
    ui_battery_label = ui_lb(sb, bat_str, 340, 3, GN_UI, F12_UI);
    lv_obj_clear_flag(ui_battery_label, LV_OBJ_FLAG_CLICKABLE);
    ui_time_label = ui_lb(sb, "12:30", 385, 3, WH_UI, F12_UI);
    lv_obj_clear_flag(ui_time_label, LV_OBJ_FLAG_CLICKABLE);

    return sb;
}

static void ui_update_battery(void) {
    if (!ui_battery_label) return;
    int pct = PMU_GetBatteryPercent();
    if (pct < 0) return;
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    lv_label_set_text(ui_battery_label, buf);
}

static void ui_update_time(void) {
    if (!ui_time_label) return;
    uint32_t now_sec = ui_boot_timestamp + (TICK_GET() - ui_boot_millis) / 1000;
    uint32_t local = now_sec + ui_tz_offset_sec;
    uint32_t h = (local / 3600) % 24;
    uint32_t m = (local / 60) % 60;
    char buf[8];
    snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)h, (unsigned)m);
    lv_label_set_text(ui_time_label, buf);
}

static void zoom_anim_cb(void *obj, int32_t v) {
    IMG_SET_SCALE((lv_obj_t *)obj, v);
}

static void ui_do_squeeze(lv_obj_t *target) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, target);
    lv_anim_set_values(&a, 256, 200);
    lv_anim_set_time(&a, 100);
    lv_anim_set_early_apply(&a, true);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)zoom_anim_cb);
    lv_anim_start(&a);

    lv_anim_t a2;
    lv_anim_init(&a2);
    lv_anim_set_var(&a2, target);
    lv_anim_set_values(&a2, 200, 256);
    lv_anim_set_time(&a2, 300);
    lv_anim_set_delay(&a2, 100);
    lv_anim_set_path_cb(&a2, lv_anim_path_bounce);
    lv_anim_set_exec_cb(&a2, (lv_anim_exec_xcb_t)zoom_anim_cb);
    lv_anim_start(&a2);
}

static void ui_hide_joke_overlay(void) {
    if (!ui_joke_label) return;
    lv_obj_t *overlay = lv_obj_get_parent(ui_joke_label);
    if (overlay) lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    ui_joke_visible = false;
}

static void ui_show_joke(const char *text) {
    if (!ui_joke_label) return;
    lv_obj_t *overlay = lv_obj_get_parent(ui_joke_label);
    if (!overlay) return;

    lv_label_set_text(ui_joke_label, text);

    lv_point_t sz;
    TXT_GET_SIZE(&sz, text, &lv_font_montserrat_20, 0, 4, 320, LV_TEXT_FLAG_NONE);
    int new_h = sz.y + 24;
    if (new_h < 50) new_h = 50;
    if (new_h > 300) new_h = 300;
    lv_obj_set_height(overlay, new_h);
    lv_obj_set_pos(overlay, 10, (360 - new_h) / 2);

#ifdef LV_USE_SDL
    lv_anim_delete(overlay, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
#endif
    lv_obj_set_style_bg_color(overlay, CLR(0x00, 0x00, 0x00), 0);
    lv_obj_set_style_bg_opa(overlay, 180, 0);
    lv_obj_set_style_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(overlay, 12, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 8, 0);
    lv_obj_set_pos(overlay, 10, (360 - new_h) / 2);
    lv_obj_set_height(overlay, new_h);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_HIDDEN);
    ui_joke_visible = true;
    lv_obj_invalidate(overlay);

#ifdef LV_USE_SDL
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, overlay);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&a, 2000);
    lv_anim_set_delay(&a, 5000);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
    lv_anim_set_completed_cb(&a, (lv_anim_completed_cb_t)ui_hide_joke_overlay);
    lv_anim_start(&a);
    lv_obj_set_pos(overlay, 10, (360 - new_h) / 2);
    lv_obj_set_height(overlay, new_h);
#else
    ui_joke_hide_at = TICK_GET() + 5000;
    lv_obj_set_pos(overlay, 10, (360 - new_h) / 2);
    lv_obj_set_height(overlay, new_h);
#endif
}

static void ui_pet_tap_handler(lv_event_t *e) {
    static uint32_t last_click = 0;
    static int click_count = 0;

    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_CLICKED) return;

    lv_obj_t *tgt = lv_event_get_target(e);
    if (tgt != ui_pet_avatar) return;

    uint32_t now = TICK_GET();
    if (now - last_click < 400)
        click_count++;
    else
        click_count = 1;
    last_click = now;

    if (click_count >= 2) {
        click_count = 0;
        static int prev = -1;
        int j;
        do { j = RAND() % JOKE_COUNT; } while (j == prev && JOKE_COUNT > 1);
        prev = j;
        ui_show_joke(jokes[j]);
    } else {
        lv_obj_t *target_img = ui_pet_image ? ui_pet_image : ui_pet_avatar;
        if (target_img) ui_do_squeeze(target_img);
    }
}

/* ========== 前向声明 ========== */
static void ui_goto_settings(lv_event_t *e);
static void ui_goto_home(lv_event_t *e);
static void ui_page_settings(lv_obj_t *p);
static void ui_show_time_popup(lv_event_t *e);
static void ui_build_time_popup(lv_obj_t *parent);
static void ui_show_about_popup(lv_event_t *e);
static void ui_build_about_popup(lv_obj_t *parent);
static void ui_pages_animate_to(int target);

/* ========== 时间浮窗 - 暂存变量 ========== */
static int32_t  ui_popup_tmp_tz   = 0;
static uint32_t ui_popup_tmp_ts   = 0;

/* ========== 时间浮窗 - 更新显示（仅浮窗内部标签，不动全局）========== */
static void ui_popup_update_preview(void) {
    if (ui_popup_tz_label) {
        char buf[16]; snprintf(buf, sizeof(buf), "GMT%+d", (int)(ui_popup_tmp_tz / 3600));
        lv_label_set_text(ui_popup_tz_label, buf);
    }
    if (ui_popup_time_label) {
        char buf[8]; snprintf(buf, sizeof(buf), "%02u:%02u",
            (uint32_t)((ui_popup_tmp_ts + ui_popup_tmp_tz) / 3600) % 24,
            (uint32_t)((ui_popup_tmp_ts + ui_popup_tmp_tz) / 60) % 60);
        lv_label_set_text(ui_popup_time_label, buf);
    }
}

/* ========== 时间浮窗按钮回调 ========== */
static void ui_popup_inc_tz(lv_event_t *e) { (void)e; if (ui_popup_tmp_tz < 14 * 3600) { ui_popup_tmp_tz += 3600; ui_popup_update_preview(); } }
static void ui_popup_dec_tz(lv_event_t *e) { (void)e; if (ui_popup_tmp_tz > -12 * 3600) { ui_popup_tmp_tz -= 3600; ui_popup_update_preview(); } }
static void ui_popup_inc_h(lv_event_t *e)  { (void)e; ui_popup_tmp_ts += 3600; ui_popup_update_preview(); }
static void ui_popup_dec_h(lv_event_t *e)  { (void)e; ui_popup_tmp_ts -= 3600; ui_popup_update_preview(); }
static void ui_popup_inc_m(lv_event_t *e)  { (void)e; ui_popup_tmp_ts += 60; ui_popup_update_preview(); }
static void ui_popup_dec_m(lv_event_t *e)  { (void)e; ui_popup_tmp_ts -= 60; ui_popup_update_preview(); }

/* ========== OK 按钮：写入全局并关闭 ========== */
static void ui_popup_ok(lv_event_t *e) {
    (void)e;
    lv_obj_t *scr = lv_scr_act();
    /* 先写入全局变量 */
    ui_tz_offset_sec = ui_popup_tmp_tz;
    ui_boot_timestamp = ui_popup_tmp_ts;
    /* 强制刷新设置页标签和状态栏 */
    if (ui_page_settings_obj && !lv_obj_has_flag(ui_page_settings_obj, LV_OBJ_FLAG_HIDDEN)) {
        if (ui_set_tz_label) {
            char buf[16]; snprintf(buf, sizeof(buf), "GMT%+d", (int)(ui_tz_offset_sec / 3600));
            lv_label_set_text(ui_set_tz_label, buf);
        }
        if (ui_set_time_label) {
            char buf[8]; snprintf(buf, sizeof(buf), "%02u:%02u",
                (uint32_t)((ui_boot_timestamp + ui_tz_offset_sec) / 3600) % 24,
                (uint32_t)((ui_boot_timestamp + ui_tz_offset_sec) / 60) % 60);
            lv_label_set_text(ui_set_time_label, buf);
        }
    }
    /* 强制刷新状态栏时间，直接用新值计算（不走 TICK_GET 偏移） */
    if (ui_time_label) {
        uint32_t local = ui_boot_timestamp + ui_tz_offset_sec;
        uint32_t h = (local / 3600) % 24;
        uint32_t m = (local / 60) % 60;
        char buf[8];
        snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)h, (unsigned)m);
        lv_label_set_text(ui_time_label, buf);
    }
    /* 关闭浮窗 */
    if (ui_time_popup) {
        lv_obj_add_flag(ui_time_popup, LV_OBJ_FLAG_HIDDEN);
    }
    /* 强制全屏重绘 */
    lv_obj_invalidate(scr);
}

/* ========== 取消/关闭：不保存，直接关闭 ========== */
static void ui_popup_cancel(lv_event_t *e) {
    (void)e;
    if (ui_time_popup) {
        /* 关闭前回存暂存值为当前全局值（丢弃调整） */
        ui_popup_tmp_tz = ui_tz_offset_sec;
        ui_popup_tmp_ts = ui_boot_timestamp;
        lv_obj_add_flag(ui_time_popup, LV_OBJ_FLAG_HIDDEN);
        lv_obj_invalidate(lv_scr_act());
    }
}

/* ========== 显示浮窗：从全局复制到暂存 ========== */
static void ui_show_time_popup(lv_event_t *e) {
    (void)e;
    if (ui_time_popup) {
        ui_popup_tmp_tz = ui_tz_offset_sec;
        ui_popup_tmp_ts = ui_boot_timestamp;
        ui_popup_update_preview();
        lv_obj_clear_flag(ui_time_popup, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(ui_time_popup);
    }
}

/* ========== 辅助：创建浮窗中一行的 +/- 按钮组（80x52 大按钮），标题居中 ========== */
static lv_obj_t *ui_popup_make_row(lv_obj_t *parent, const char *title, int y,
    lv_event_cb_t minus_cb, lv_event_cb_t plus_cb, lv_obj_t **value_label) {
    /* 标题：居中于按钮组之间（- 按钮右端 105 到 + 按钮左端 260，中心约 182）
     * 宽度留够避免折行，手动定位 + text_align center */
    lv_obj_t *title_lbl = lv_label_create(parent);
    lv_label_set_text(title_lbl, title);
    lv_obj_set_style_text_color(title_lbl, WH_UI, 0);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(title_lbl, 120, y);
    lv_obj_set_style_text_align(title_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(title_lbl, 140);

    int btn_y = y + 28;

    /* "-" 按钮 80x52 */
    lv_obj_t *minus = lv_btn_create(parent);
    lv_obj_set_size(minus, 80, 52);
    lv_obj_set_pos(minus, 25, btn_y);
    lv_obj_set_style_bg_color(minus, CLR(0x22,0x22,0x3A), 0);
    lv_obj_set_style_radius(minus, 8, 0);
    lv_obj_set_style_border_width(minus, 0, 0);
    lv_obj_t *minus_lbl = lv_label_create(minus);
    lv_label_set_text(minus_lbl, "-");
    lv_obj_set_style_text_font(minus_lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(minus_lbl, WH_UI, 0);
    lv_obj_center(minus_lbl);
    lv_obj_add_event_cb(minus, minus_cb, LV_EVENT_CLICKED, NULL);

    if (value_label) {
        *value_label = ui_lb(parent, "--", 110, btn_y + 6, CY_UI, F28_UI);
        lv_obj_set_style_text_align(*value_label, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(*value_label, 140);
    }

    /* "+" 按钮 80x52 */
    lv_obj_t *plus = lv_btn_create(parent);
    lv_obj_set_size(plus, 80, 52);
    lv_obj_set_pos(plus, 260, btn_y);
    lv_obj_set_style_bg_color(plus, CLR(0x22,0x22,0x3A), 0);
    lv_obj_set_style_radius(plus, 8, 0);
    lv_obj_set_style_border_width(plus, 0, 0);
    lv_obj_t *plus_lbl = lv_label_create(plus);
    lv_label_set_text(plus_lbl, "+");
    lv_obj_set_style_text_font(plus_lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(plus_lbl, CY_UI, 0);
    lv_obj_center(plus_lbl);
    lv_obj_add_event_cb(plus, plus_cb, LV_EVENT_CLICKED, NULL);

    return minus;
}



/* ========== 构建时间设置浮窗 ========== */
static void ui_build_time_popup(lv_obj_t *parent) {
    lv_obj_t *mask = lv_obj_create(parent);
    lv_obj_set_size(mask, 480, 480);
    lv_obj_set_pos(mask, 0, 0);
    lv_obj_set_style_bg_color(mask, CLR(0x00,0x00,0x00), 0);
    lv_obj_set_style_bg_opa(mask, 140, 0);
    lv_obj_set_style_border_width(mask, 0, 0);
    lv_obj_set_style_pad_all(mask, 0, 0);
    lv_obj_set_scrollbar_mode(mask, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *pop = lv_obj_create(mask);
    lv_obj_set_size(pop, 380, 420);
    lv_obj_set_pos(pop, 50, 50);
    lv_obj_set_style_bg_color(pop, CLR(0x14,0x14,0x24), 0);
    lv_obj_set_style_bg_opa(pop, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(pop, 16, 0);
    lv_obj_set_style_border_width(pop, 1, 0);
    lv_obj_set_style_border_color(pop, CY_UI, 0);
    lv_obj_set_style_pad_all(pop, 0, 0);
    lv_obj_set_scrollbar_mode(pop, LV_SCROLLBAR_MODE_OFF);

    /* 标题 */
    ui_lb(pop, "Time & Timezone", 25, 20, CY_UI, &lv_font_montserrat_20);

    /* 时区行：- [GMT+8] + */
    ui_popup_make_row(pop, "Timezone", 60, ui_popup_dec_tz, ui_popup_inc_tz, &ui_popup_tz_label);

    /* 小时行 */
    ui_popup_make_row(pop, "Hour", 160, ui_popup_dec_h, ui_popup_inc_h, NULL);
    /* 大号时间居中显示 */
    ui_popup_time_label = ui_lb(pop, "00:00", 110, 188, CY_UI, F28_UI);
    lv_obj_set_style_text_align(ui_popup_time_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(ui_popup_time_label, 140);

    /* 分钟行 */
    ui_popup_make_row(pop, "Minute", 245, ui_popup_dec_m, ui_popup_inc_m, NULL);

    /* OK 按钮（确认，保存并关闭） */
    lv_obj_t *ok_btn = lv_btn_create(pop);
    lv_obj_set_size(ok_btn, 140, 50);
    lv_obj_set_pos(ok_btn, 40, 350);
    lv_obj_set_style_bg_color(ok_btn, CY_UI, 0);
    lv_obj_set_style_radius(ok_btn, 25, 0);
    lv_obj_set_style_border_width(ok_btn, 0, 0);
    lv_obj_t *ok_lbl = lv_label_create(ok_btn);
    lv_label_set_text(ok_lbl, "OK");
    lv_obj_set_style_text_color(ok_lbl, CLR(0x0D,0x0D,0x1A), 0);
    lv_obj_set_style_text_font(ok_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(ok_lbl);
    lv_obj_add_event_cb(ok_btn, ui_popup_ok, LV_EVENT_CLICKED, NULL);

    /* CANCEL 按钮（取消，不保存关闭） */
    lv_obj_t *cancel_btn = lv_btn_create(pop);
    lv_obj_set_size(cancel_btn, 140, 50);
    lv_obj_set_pos(cancel_btn, 200, 350);
    lv_obj_set_style_bg_color(cancel_btn, CLR(0x3A,0x3A,0x3A), 0);
    lv_obj_set_style_radius(cancel_btn, 25, 0);
    lv_obj_set_style_border_width(cancel_btn, 1, 0);
    lv_obj_set_style_border_color(cancel_btn, BD_UI, 0);
    lv_obj_t *cancel_lbl = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_lbl, "Cancel");
    lv_obj_set_style_text_color(cancel_lbl, WH_UI, 0);
    lv_obj_set_style_text_font(cancel_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(cancel_lbl);
    lv_obj_add_event_cb(cancel_btn, ui_popup_cancel, LV_EVENT_CLICKED, NULL);

    /* 点击遮罩 = 取消，不保存 */
    lv_obj_add_flag(mask, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(mask, ui_popup_cancel, LV_EVENT_CLICKED, NULL);

    lv_obj_add_flag(mask, LV_OBJ_FLAG_HIDDEN);
    ui_time_popup = mask;
}

/* ========== 第0页：爱因斯坦 ========== */
static void ui_page0(lv_obj_t *p) {
    lv_obj_t *avatar = lv_obj_create(p);
    ui_pet_avatar = avatar;
    lv_obj_set_size(avatar, 360, 360);
    lv_obj_set_pos(avatar, 60, 60);
    lv_obj_set_style_bg_color(avatar, BG_UI, 0);
    lv_obj_set_style_bg_opa(avatar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(avatar, 180, 0);
    lv_obj_set_style_border_width(avatar, 3, 0);
    lv_obj_set_style_border_color(avatar, CY_UI, 0);
    lv_obj_set_style_pad_all(avatar, 0, 0);
    lv_obj_set_scrollbar_mode(avatar, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(avatar, LV_OBJ_FLAG_SCROLLABLE);

#ifndef LV_USE_SDL
    lv_obj_set_style_clip_corner(avatar, true, 0);
#endif
    CLIP_CHILDREN(avatar);

    lv_obj_add_flag(avatar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(avatar, ui_pet_tap_handler, LV_EVENT_CLICKED, NULL);

    lv_obj_t *img = IMG_CREATE(avatar);
    ui_pet_image = img;
    IMG_SET_SRC(img, &einstein_img_dsc);
    lv_obj_set_size(img, 280, 280);
    lv_obj_set_pos(img, 40, 40);
    IMG_SET_SCALE(img, 256);
    SET_STYLE_IMG_OPA(img, LV_OPA_COVER, 0);

    /* 笑话覆盖层 */
    lv_obj_t *overlay = lv_obj_create(avatar);
    lv_obj_set_size(overlay, 340, 50);
    lv_obj_set_pos(overlay, 10, 155);
    lv_obj_set_style_bg_color(overlay, CLR(0x00, 0x00, 0x00), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(overlay, 12, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 8, 0);
    lv_obj_set_scrollbar_mode(overlay, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *lbl = lv_label_create(overlay);
    ui_joke_label = lbl;
    lv_label_set_text(lbl, "");
    lv_obj_set_style_text_color(lbl, WH_UI, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, 320);

}

/* ====== 浮窗构建函数移到 page0 外，已由 ui_build_all 调用 ====== */

/* ========== 第1页：设置 ========== */
static void ui_page_settings(lv_obj_t *p) {
    lv_obj_set_style_bg_color(p, BG_UI, 0);

    /* 标题居中 */
    lv_obj_t *title_lbl = lv_label_create(p);
    lv_label_set_text(title_lbl, "Settings");
    lv_obj_set_style_text_color(title_lbl, CY_UI, 0);
    lv_obj_set_style_text_font(title_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(title_lbl);
    lv_obj_set_y(title_lbl, 60);

    /* Time 按钮 —— 点击进入时间浮窗 */
    lv_obj_t *time_btn = lv_btn_create(p);
    lv_obj_set_size(time_btn, 360, 80);
    lv_obj_set_pos(time_btn, 60, 130);
    lv_obj_set_style_bg_color(time_btn, CLR(0x1A,0x1A,0x2E), 0);
    lv_obj_set_style_radius(time_btn, 16, 0);
    lv_obj_set_style_border_width(time_btn, 1, 0);
    lv_obj_set_style_border_color(time_btn, BD_UI, 0);
    lv_obj_set_style_pad_all(time_btn, 0, 0);
    ui_lb(time_btn, "Time", 20, 16, WH_UI, &lv_font_montserrat_20);
    /* 右侧显示当前时区 + 时间预览 */
    char tz_buf[16];
    snprintf(tz_buf, sizeof(tz_buf), "GMT%+d", (int)(ui_tz_offset_sec / 3600));
    ui_set_tz_label = ui_lb(time_btn, tz_buf, 240, 18, CY_UI, &lv_font_montserrat_16);
    char time_buf[8];
    snprintf(time_buf, sizeof(time_buf), "%02u:%02u",
        (uint32_t)((ui_boot_timestamp + ui_tz_offset_sec) / 3600) % 24,
        (uint32_t)((ui_boot_timestamp + ui_tz_offset_sec) / 60) % 60);
    ui_set_time_label = ui_lb(time_btn, time_buf, 270, 38, CY_UI, &lv_font_montserrat_20);
    lv_obj_add_event_cb(time_btn, ui_show_time_popup, LV_EVENT_CLICKED, NULL);

    /* About 按钮 —— 点击弹关于页 */
    lv_obj_t *about_btn = lv_btn_create(p);
    lv_obj_set_size(about_btn, 360, 80);
    lv_obj_set_pos(about_btn, 60, 230);
    lv_obj_set_style_bg_color(about_btn, CLR(0x1A,0x1A,0x2E), 0);
    lv_obj_set_style_radius(about_btn, 16, 0);
    lv_obj_set_style_border_width(about_btn, 1, 0);
    lv_obj_set_style_border_color(about_btn, BD_UI, 0);
    lv_obj_set_style_pad_all(about_btn, 0, 0);
    ui_lb(about_btn, "About", 20, 28, WH_UI, &lv_font_montserrat_20);
    lv_obj_add_event_cb(about_btn, ui_show_about_popup, LV_EVENT_CLICKED, NULL);
}

/* ========== About 浮窗 ========== */
static lv_obj_t *ui_about_popup = NULL;
static void ui_close_about_popup(lv_event_t *e) {
    (void)e;
    if (ui_about_popup) lv_obj_add_flag(ui_about_popup, LV_OBJ_FLAG_HIDDEN);
}
static void ui_show_about_popup(lv_event_t *e) {
    (void)e;
    if (!ui_about_popup) return;
    lv_obj_clear_flag(ui_about_popup, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ui_about_popup);
}
static void ui_build_about_popup(lv_obj_t *parent) {
    lv_obj_t *mask = lv_obj_create(parent);
    lv_obj_set_size(mask, 480, 480);
    lv_obj_set_pos(mask, 0, 0);
    lv_obj_set_style_bg_color(mask, CLR(0x00,0x00,0x00), 0);
    lv_obj_set_style_bg_opa(mask, 140, 0);
    lv_obj_set_style_border_width(mask, 0, 0);
    lv_obj_set_style_pad_all(mask, 0, 0);
    lv_obj_set_scrollbar_mode(mask, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *pop = lv_obj_create(mask);
    lv_obj_set_size(pop, 360, 360);
    lv_obj_set_pos(pop, 60, 60);
    lv_obj_set_style_bg_color(pop, CLR(0x14,0x14,0x24), 0);
    lv_obj_set_style_bg_opa(pop, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(pop, 16, 0);
    lv_obj_set_style_border_width(pop, 1, 0);
    lv_obj_set_style_border_color(pop, CY_UI, 0);
    lv_obj_set_style_pad_all(pop, 0, 0);
    lv_obj_set_scrollbar_mode(pop, LV_SCROLLBAR_MODE_OFF);

    /* 标题 */
    lv_obj_t *t = lv_label_create(pop);
    lv_label_set_text(t, "About");
    lv_obj_set_style_text_color(t, CY_UI, 0);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
    lv_obj_set_pos(t, 30, 30);

    /* 内容 */
    lv_obj_t *l1 = lv_label_create(pop);
    lv_label_set_text(l1, "Desk-Mate");
    lv_obj_set_style_text_color(l1, WH_UI, 0);
    lv_obj_set_style_text_font(l1, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(l1, 30, 80);

    lv_obj_t *l2 = lv_label_create(pop);
    lv_label_set_text(l2, "ESP32-C6 AMOLED Pet");
    lv_obj_set_style_text_color(l2, BD_UI, 0);
    lv_obj_set_style_text_font(l2, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(l2, 30, 110);

    lv_obj_t *l3 = lv_label_create(pop);
    lv_label_set_text(l3, "Hardware: ESP32-C6");
    lv_obj_set_style_text_color(l3, WH_UI, 0);
    lv_obj_set_style_text_font(l3, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(l3, 30, 160);

    lv_obj_t *l4 = lv_label_create(pop);
    lv_label_set_text(l4, "Display: 2.16\" AMOLED");
    lv_obj_set_style_text_color(l4, WH_UI, 0);
    lv_obj_set_style_text_font(l4, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(l4, 30, 190);

    lv_obj_t *l5 = lv_label_create(pop);
    lv_label_set_text(l5, "FW: v1.0.0");
    lv_obj_set_style_text_color(l5, WH_UI, 0);
    lv_obj_set_style_text_font(l5, &lv_font_montserrat_14, 0);
    lv_obj_set_pos(l5, 30, 220);

    /* OK 按钮 */
    lv_obj_t *ok = lv_btn_create(pop);
    lv_obj_set_size(ok, 140, 44);
    lv_obj_set_pos(ok, 110, 290);
    lv_obj_set_style_bg_color(ok, CY_UI, 0);
    lv_obj_set_style_radius(ok, 22, 0);
    lv_obj_set_style_border_width(ok, 0, 0);
    lv_obj_t *ok_lbl = lv_label_create(ok);
    lv_label_set_text(ok_lbl, "Close");
    lv_obj_set_style_text_color(ok_lbl, CLR(0x0D,0x0D,0x1A), 0);
    lv_obj_set_style_text_font(ok_lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(ok_lbl);
    lv_obj_add_event_cb(ok, ui_close_about_popup, LV_EVENT_CLICKED, NULL);

    /* 点击遮罩关闭 */
    lv_obj_add_flag(mask, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(mask, ui_close_about_popup, LV_EVENT_CLICKED, NULL);

    lv_obj_add_flag(mask, LV_OBJ_FLAG_HIDDEN);
    ui_about_popup = mask;
}

/* ========== 页面切换：左右滑动 ==========
 * 两页并存，x 偏移由 0 (home) 和 -480 (settings) 决定。
 * 滑动跟随手指，松手阈值 > 80px 切换页面，否则弹回。
 * 同时使用一个透明全屏触控面板接收触摸事件（不于扰内部交互）。 */
#define UI_SWIPE_THRESHOLD 80
static lv_obj_t *ui_swipe_panel = NULL;
static int32_t ui_drag_start_x = 0;
static bool ui_dragging = false;
static lv_obj_t *ui_page_indicator[2] = {NULL, NULL};

/* 动画执行回调：跟随手指 */
static void ui_swipe_anim_exec(lv_obj_t *obj, int32_t v) {
    lv_obj_set_x(obj, v);
}

/* 重置所有页面到最终位置（动画）
 * - ui_current_page: 0=HOME → 两页 x 0 / -480
 * - 1=SETTINGS → 两页 x 480 / 0 */
static void ui_pages_animate_to(int target_page) {
    int32_t home_x = (target_page == 0) ? 0 : -480;
    int32_t set_x  = (target_page == 0) ? 480 : 0;

    lv_anim_t a1, a2;
    lv_anim_init(&a1);
    lv_anim_set_var(&a1, ui_page_home_obj);
    lv_anim_set_values(&a1, lv_obj_get_x(ui_page_home_obj), home_x);
    lv_anim_set_time(&a1, 250);
    lv_anim_set_path_cb(&a1, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a1, (lv_anim_exec_xcb_t)ui_swipe_anim_exec);
    lv_anim_start(&a1);

    lv_anim_init(&a2);
    lv_anim_set_var(&a2, ui_page_settings_obj);
    lv_anim_set_values(&a2, lv_obj_get_x(ui_page_settings_obj), set_x);
    lv_anim_set_time(&a2, 250);
    lv_anim_set_path_cb(&a2, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a2, (lv_anim_exec_xcb_t)ui_swipe_anim_exec);
    lv_anim_start(&a2);

    ui_current_page = target_page;
    /* 指示器更新 */
    for (int i = 0; i < 2; i++) {
        if (ui_page_indicator[i]) {
            lv_obj_set_style_bg_color(ui_page_indicator[i],
                (i == target_page) ? CY_UI : BD_UI, 0);
        }
    }
}

/* 滑动手势事件 */
static void ui_swipe_event(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;

    if (code == LV_EVENT_PRESSED) {
        ui_drag_start_x = lv_indev_get_gesture_dir(indev); /* unused */
        lv_point_t p;
        lv_indev_get_point(indev, &p);
        ui_drag_start_x = p.x;
        ui_dragging = true;
        /* 取消所有动画以跟随手指 */
#ifdef LV_USE_SDL
        lv_anim_delete(ui_page_home_obj, NULL);
        lv_anim_delete(ui_page_settings_obj, NULL);
#else
        lv_anim_del(ui_page_home_obj, NULL);
        lv_anim_del(ui_page_settings_obj, NULL);
#endif
    } else if (code == LV_EVENT_PRESSING && ui_dragging) {
        lv_point_t p;
        lv_indev_get_point(indev, &p);
        int32_t dx = p.x - ui_drag_start_x;
        int32_t home_base = (ui_current_page == 0) ? 0 : -480;
        int32_t set_base  = (ui_current_page == 0) ? 480 : 0;
        int32_t new_home_x = home_base + dx;
        int32_t new_set_x  = set_base + dx;
        /* 限制越界 */
        if (new_home_x > 0) new_home_x = 0;
        if (new_home_x < -480) new_home_x = -480;
        if (new_set_x > 480) new_set_x = 480;
        if (new_set_x < 0) new_set_x = 0;
        lv_obj_set_x(ui_page_home_obj, new_home_x);
        lv_obj_set_x(ui_page_settings_obj, new_set_x);
    } else if ((code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) && ui_dragging) {
        ui_dragging = false;
        int32_t home_x = lv_obj_get_x(ui_page_home_obj);
        int32_t set_x  = lv_obj_get_x(ui_page_settings_obj);
        int32_t target = ui_current_page;
        /* 根据偏移量判断要跳到哪页 */
        if (ui_current_page == 0) {
            /* 在 HOME，右滑回去；左滑超过阈值切到 SETTINGS */
            if (home_x < -UI_SWIPE_THRESHOLD) target = 1;
            else target = 0;
        } else {
            /* 在 SETTINGS，左滑回去；右滑超过阈值切到 HOME */
            if (set_x > UI_SWIPE_THRESHOLD) target = 0;
            else target = 1;
        }
        ui_pages_animate_to(target);
    }
}

/* 兼容旧名字的函数 */
static void ui_goto_settings(lv_event_t *e) { (void)e; ui_pages_animate_to(1); }
static void ui_goto_home(lv_event_t *e) { (void)e; ui_pages_animate_to(0); }

/* ========== 构建全部 UI ========== */
void ui_build_all(lv_obj_t *scr) {
    lv_obj_set_style_bg_color(scr, BG_UI, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    /* 关闭 scr 默认滚动条（避免出现意外的横向滚动条）*/
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

#ifdef LV_USE_SDL
    font_tnr_20.fallback = &font_cn_20;
    font_cn_20.fallback = &lv_font_montserrat_20;
    font_cn_28.fallback = &lv_font_montserrat_24;
#endif

    ui_build_sb(scr);

    /* HOME 页初始 x=0 */
    ui_page_home_obj = lv_obj_create(scr);
    lv_obj_set_size(ui_page_home_obj, 480, 480 - 28);
    lv_obj_set_pos(ui_page_home_obj, 0, 28);
    lv_obj_set_style_bg_opa(ui_page_home_obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ui_page_home_obj, 0, 0);
    lv_obj_set_style_pad_all(ui_page_home_obj, 0, 0);
    lv_obj_set_scrollbar_mode(ui_page_home_obj, LV_SCROLLBAR_MODE_OFF);
    ui_page0(ui_page_home_obj);
    /* 滑动事件绑到 page 上 */
    lv_obj_add_event_cb(ui_page_home_obj, ui_swipe_event, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(ui_page_home_obj, ui_swipe_event, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(ui_page_home_obj, ui_swipe_event, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(ui_page_home_obj, ui_swipe_event, LV_EVENT_PRESS_LOST, NULL);

    /* SETTINGS 页初始 x=480（右侧隐藏）*/
    ui_page_settings_obj = lv_obj_create(scr);
    lv_obj_set_size(ui_page_settings_obj, 480, 480 - 28);
    lv_obj_set_pos(ui_page_settings_obj, 480, 28);
    lv_obj_set_style_bg_opa(ui_page_settings_obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ui_page_settings_obj, 0, 0);
    lv_obj_set_style_pad_all(ui_page_settings_obj, 0, 0);
    lv_obj_set_scrollbar_mode(ui_page_settings_obj, LV_SCROLLBAR_MODE_OFF);
    ui_page_settings(ui_page_settings_obj);
    lv_obj_add_event_cb(ui_page_settings_obj, ui_swipe_event, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(ui_page_settings_obj, ui_swipe_event, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(ui_page_settings_obj, ui_swipe_event, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(ui_page_settings_obj, ui_swipe_event, LV_EVENT_PRESS_LOST, NULL);

    /* Page indicator（底部两个点）*/
    for (int i = 0; i < 2; i++) {
        ui_page_indicator[i] = lv_obj_create(scr);
        lv_obj_set_size(ui_page_indicator[i], 12, 12);
        lv_obj_set_pos(ui_page_indicator[i], 220 + i * 28, 450);
        lv_obj_set_style_radius(ui_page_indicator[i], 6, 0);
        lv_obj_set_style_border_width(ui_page_indicator[i], 0, 0);
        lv_obj_set_style_bg_opa(ui_page_indicator[i], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(ui_page_indicator[i], (i == 0) ? CY_UI : BD_UI, 0);
        lv_obj_clear_flag(ui_page_indicator[i], LV_OBJ_FLAG_CLICKABLE);
    }

    /* 时间浮窗（跨页面可见）*/
    ui_build_time_popup(scr);
    /* About 浮窗 */
    ui_build_about_popup(scr);

    /* 最后再次把状态栏置顶——以防后续创建对象把 sb 压在下面 */
    if (ui_time_label) {
        lv_obj_t *sb = lv_obj_get_parent(ui_time_label);
        if (sb) lv_obj_move_foreground(sb);
        lv_obj_move_foreground(ui_time_label);
        lv_obj_move_foreground(ui_battery_label);
    }

    ui_update_time();
}

#endif /* DESKMATE_UI_IMPL */
#endif /* UI_PAGES_H */
