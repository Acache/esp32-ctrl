/**
 * ui_pages.h — Desk-Mate UI 源码（模拟器 v9 + 硬件 v8 统一版本）
 * 
 * ═══════════════════════════════════════════════════════════════════
 * 核心理念：模拟器 = 硬件，同一份代码，同一份效果
 * 区分方式：#ifdef LV_USE_SDL → 模拟器(LVGL v9 API)，否则 → 硬件(LVGL v8 API)
 * ═══════════════════════════════════════════════════════════════════
 * 
 * 功能：
 * - 爱因斯坦头像（圆形容器中）
 * - 单击头像 → 呼吸缩放（100%→78%→100%，带bounce反弹）
 * - 双击头像 → 显示随机笑话（5秒后自动隐藏）
 * - 顶部状态栏（设备名、电量、时间）
 * 
 * 布局：
 * - 状态栏：高度 28px，顶对齐
 * - 头像容器：360x360，位置 (60, 60)，圆角 180 = 正圆
 * - 图片：320x320，居中于容器 (20, 20)
 * - 笑话覆盖层：340x50～300，居中于容器
 */
#ifndef UI_PAGES_H
#define UI_PAGES_H

#ifdef DESKMATE_UI_IMPL

#include "jokes.h"
#include "einstein_img.h"

/* ====== API 兼容层 ======
 * 模拟器（LV_USE_SDL 有定义）→ LVGL v9 API
 * 硬件 → LVGL v8 API
 */
#ifdef LV_USE_SDL
  #define IMG_CREATE(p)       lv_image_create(p)
  #define IMG_SET_SRC(o, s)   lv_image_set_src(o, s)
  #define IMG_SET_SCALE(o, v) lv_image_set_scale(o, v)
  #define TICK_GET()          lv_tick_get()
  #define RAND()              rand()
  #define TXT_GET_SIZE(s, t, f, ls, l, w, fl) lv_text_get_size(s, t, f, ls, l, w, fl)
  #define SET_STYLE_IMG_OPA(o, opa, sel)       lv_obj_set_style_image_opa(o, opa, sel)
  #define OBJ_ADD_FLAG(o, f)  lv_obj_add_flag(o, f)
  #define OBJ_CLEAR_FLAG(o, f) lv_obj_clear_flag(o, f)
  #define CLIP_CHILDREN(o)    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE)
#else
  #define IMG_CREATE(p)       lv_img_create(p)
  #define IMG_SET_SRC(o, s)   lv_img_set_src(o, s)
  #define IMG_SET_SCALE(o, v) lv_img_set_zoom(o, v)
  #define TICK_GET()          millis()
  #define RAND()              random()
  #define TXT_GET_SIZE(s, t, f, ls, l, w, fl) lv_txt_get_size(s, t, f, ls, l, w, fl)
  #define SET_STYLE_IMG_OPA(o, opa, sel)       lv_obj_set_style_img_opa(o, opa, sel)
  #define OBJ_ADD_FLAG(o, f)  lv_obj_add_flag(o, f)
  #define OBJ_CLEAR_FLAG(o, f) lv_obj_clear_flag(o, f)
  #define CLIP_CHILDREN(o)    /* v8中使用style clip_corner直接设置，见page0 */
#endif

/* ====== 字体 ====== */
#ifdef LV_USE_SDL
extern lv_font_t font_tnr_20;
extern lv_font_t font_cn_20;
extern lv_font_t font_cn_28;
#define F20_UI &font_tnr_20
#define F28_UI &font_cn_28
#else
#define F20_UI &lv_font_montserrat_20
#define F28_UI &lv_font_montserrat_20
#endif

/* ========== 颜色 ========== */
#define CLR(r,g,b) lv_color_make(r,g,b)
#define BG_UI  CLR(0x0D,0x0D,0x1A)
#define SB_UI  CLR(0x0A,0x0A,0x14)
#define CY_UI  CLR(0x00,0xD4,0xFF)
#define GN_UI  CLR(0x00,0xE6,0x76)
#define WH_UI  CLR(0xEE,0xEE,0xEE)
#define BD_UI  CLR(0x22,0x22,0x3A)

#define F12_UI &lv_font_montserrat_16
#define F14_UI &lv_font_montserrat_14
#define F16_UI &lv_font_montserrat_16

/* 全局对象引用 */
static lv_obj_t *ui_pet_avatar = NULL;
static lv_obj_t *ui_pet_image = NULL;
static lv_obj_t *ui_joke_label = NULL;
static bool ui_joke_visible = false;
static uint32_t ui_joke_hide_at = 0;

/* ========== 辅助函数 ========== */
static lv_obj_t *ui_lb(lv_obj_t *p, const char *t, int x, int y, lv_color_t c, const lv_font_t *f) {
    lv_obj_t *l = lv_label_create(p);
    lv_label_set_text(l, t);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_style_text_color(l, c, 0);
    if (f) lv_obj_set_style_text_font(l, f, 0);
    return l;
}

/* ========== 状态栏 ========== */
static lv_obj_t *ui_build_sb(lv_obj_t *scr) {
    lv_obj_t *sb = lv_obj_create(scr);
    lv_obj_set_size(sb, 480, 28);
    lv_obj_set_pos(sb, 0, 0);
    lv_obj_set_style_bg_color(sb, SB_UI, 0);
    lv_obj_set_style_bg_opa(sb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sb, 0, 0);
    lv_obj_set_style_pad_all(sb, 0, 0);
    lv_obj_set_scrollbar_mode(sb, LV_SCROLLBAR_MODE_OFF);
    ui_lb(sb, "Desk-Mate", 40, 3, CY_UI, F12_UI);
    ui_lb(sb, "83%", 340, 3, GN_UI, F12_UI);
    ui_lb(sb, "12:30", 385, 3, WH_UI, F12_UI);

    /* 状态栏分割线 */
    lv_obj_t *se = lv_obj_create(scr);
    lv_obj_set_size(se, 480, 1);
    lv_obj_set_pos(se, 0, 28);
    lv_obj_set_style_bg_color(se, BD_UI, 0);
    lv_obj_set_style_border_width(se, 0, 0);
    return sb;
}

/* ========== 呼吸缩放 ========== */
static void zoom_anim_cb(void *obj, int32_t v) {
    IMG_SET_SCALE((lv_obj_t *)obj, v);
}

static void ui_do_squeeze(lv_obj_t *target) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, target);
    lv_anim_set_values(&a, 256, 200);     /* 100% → 78% */
    lv_anim_set_time(&a, 100);
    lv_anim_set_early_apply(&a, true);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)zoom_anim_cb);
    lv_anim_start(&a);

    lv_anim_t a2;
    lv_anim_init(&a2);
    lv_anim_set_var(&a2, target);
    lv_anim_set_values(&a2, 200, 256);    /* 78% → 100% */
    lv_anim_set_time(&a2, 300);
    lv_anim_set_delay(&a2, 100);
    lv_anim_set_path_cb(&a2, lv_anim_path_bounce);
    lv_anim_set_exec_cb(&a2, (lv_anim_exec_xcb_t)zoom_anim_cb);
    lv_anim_start(&a2);
}

/* ========== 笑话显示 ========== */
/* 隐藏笑话覆盖层 */
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

    /* 自适应高度 */
    lv_point_t sz;
    TXT_GET_SIZE(&sz, text, &lv_font_montserrat_20, 0, 4, 320, LV_TEXT_FLAG_NONE);
    int new_h = sz.y + 24;
    if (new_h < 50) new_h = 50;
    if (new_h > 300) new_h = 300;
    lv_obj_set_height(overlay, new_h);
    lv_obj_set_pos(overlay, 10, (360 - new_h) / 2);

    /* 先清除旧动画 */
#ifdef LV_USE_SDL
    lv_anim_delete(overlay, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
#else
    /* 硬件用 timer，不需要 lv_anim_del */
#endif
    /* 完整重置所有样式 */
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

    /* 5秒渐隐（模拟器用 anim，硬件用状态检查） */
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
    /* 硬件：记录隐藏时间，由 loop 中的轮询处理 */
    ui_joke_hide_at = TICK_GET() + 5000;
    lv_obj_set_pos(overlay, 10, (360 - new_h) / 2);
    lv_obj_set_height(overlay, new_h);
#endif
}

/* ========== 单击/双击事件处理器 ========== */
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

/* ========== 第0页：爱因斯坦 ========== */
static void ui_page0(lv_obj_t *p) {
    /* ---- 头像圆形容器 ---- */
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
    /* 关键：移除 SCROLLABLE 标志，否则 CLICKED 被抑制 */
    lv_obj_clear_flag(avatar, LV_OBJ_FLAG_SCROLLABLE);

    /* 裁剪子对象到圆角边界（防止头发/身体溢出圆形） */
#ifndef LV_USE_SDL
    /* 硬件 v8：通过 clip_corner 裁剪 */
    lv_obj_set_style_clip_corner(avatar, true, 0);
#endif
    CLIP_CHILDREN(avatar);

    /* 点击事件绑定在容器上 */
    lv_obj_add_flag(avatar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(avatar, ui_pet_tap_handler, LV_EVENT_CLICKED, NULL);

    /* ---- 爱因斯坦图片 ---- */
    lv_obj_t *img = IMG_CREATE(avatar);
    ui_pet_image = img;
    IMG_SET_SRC(img, &einstein_img_dsc);
    /* 图片源数据 280x280，显示大小匹配源图，居中于 360x360 容器 */
    lv_obj_set_size(img, 280, 280);
    lv_obj_set_pos(img, 40, 40);
    IMG_SET_SCALE(img, 256);
    SET_STYLE_IMG_OPA(img, LV_OPA_COVER, 0);

    /* ---- 笑话覆盖层 ---- */
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

/* ========== 构建全部 UI ========== */
void ui_build_all(lv_obj_t *scr) {
    lv_obj_set_style_bg_color(scr, BG_UI, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

#ifdef LV_USE_SDL
    /* 模拟器：设置字体 fallback 链 */
    font_tnr_20.fallback = &font_cn_20;
    font_cn_20.fallback = &lv_font_montserrat_20;
    font_cn_28.fallback = &lv_font_montserrat_28;
#endif

    ui_build_sb(scr);
    ui_page0(scr);
}

#endif /* DESKMATE_UI_IMPL */
#endif /* UI_PAGES_H */
