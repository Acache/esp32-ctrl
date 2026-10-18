#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <SDL.h>
#include "lvgl.h"
#include "../../firmware/desk-mate-app/focus_ui.h"
static uint32_t sim_tick;
static lv_point_t sim_point;
static bool sim_pressed;
static int media_count;
static uint16_t last_media;
static uint32_t test_clock(void) { return sim_tick; }
static bool sim_media(uint16_t u) { ++media_count; last_media = u; return true; }
static int sim_battery(void) { return 85; }
static FocusBiliStatus sim_bili;
static void sim_read_bili(FocusBiliStatus *out) { *out = sim_bili; }
#if LVGL_VERSION_MAJOR >= 9
static void flush(lv_display_t *d, const lv_area_t *a, uint8_t *p) {
    (void)a; (void)p; lv_display_flush_ready(d);
}
static void read_pointer(lv_indev_t *d, lv_indev_data_t *v) {
#else
#define lv_screen_active lv_scr_act
static void flush(lv_disp_drv_t *d, const lv_area_t *a, lv_color_t *p) {
    (void)a; (void)p; lv_disp_flush_ready(d);
}
static void read_pointer(lv_indev_drv_t *d, lv_indev_data_t *v) {
#endif
    (void)d; v->point = sim_point;
    v->state = sim_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
static void advance(unsigned ms) {
    for (unsigned i = 0; i < ms; i += 5) {
        sim_tick += 5;
#if LVGL_VERSION_MAJOR < 9
        lv_tick_inc(5);
#endif
        lv_timer_handler();
    }
}
static void pointer(int x, int y, bool down, unsigned ms) {
    sim_point.x = x; sim_point.y = y; sim_pressed = down; advance(ms);
}
static void drag(int x0, int x1, int y, int steps) {
    pointer(x0, y, true, 40);
    for (int i = 1; i <= steps; ++i) pointer(x0 + (x1 - x0) * i / steps, y, true, 20);
    pointer(x1, y, true, 160);
    pointer(x1, y, false, 1200);
}
static void drag_xy(int x0, int y0, int x1, int y1, int steps) {
    pointer(x0, y0, true, 40);
    for (int i = 1; i <= steps; ++i)
        pointer(x0 + (x1 - x0) * i / steps, y0 + (y1 - y0) * i / steps, true, 20);
    pointer(x1, y1, false, 400);
}
static void expect_page(int page) {
    int x = lv_obj_get_scroll_x(focus_pager);
    if (x != page * 480) fprintf(stderr, "Expected page %d at %d, got %d\n", page, page * 480, x);
    assert(x == page * 480);
    assert(focus_page == page);
}
static void run_tests(void) {
    advance(100); expect_page(0);
    const lv_font_t *timer_font = lv_obj_get_style_text_font(focus_minute_digits, LV_PART_MAIN);
    assert(timer_font && timer_font->line_height >= 88);
    assert(strcmp(lv_label_get_text(focus_state), "FOCUS") == 0);
    assert(strcmp(lv_label_get_text(focus_minute_digits), "25") == 0);
    assert(strcmp(lv_label_get_text(focus_second_digits), "00") == 0);
    assert(strcmp(lv_label_get_text(focus_wheel_prev[0]), "24") == 0);
    assert(strcmp(lv_label_get_text(focus_wheel_next[0]), "26") == 0);
    assert(strcmp(lv_label_get_text(focus_wheel_prev[1]), "59") == 0);
    assert(strcmp(lv_label_get_text(focus_wheel_next[1]), "01") == 0);
    const lv_font_t *shadow_font = lv_obj_get_style_text_font(focus_wheel_prev[0], LV_PART_MAIN);
    assert(shadow_font && shadow_font->line_height >= 45);
    assert(lv_obj_get_y(focus_state) < lv_obj_get_y(focus_minute_digits));
    assert(lv_obj_get_y(focus_minute_digits) < lv_obj_get_y(focus_caption));
    assert(lv_obj_get_y(focus_caption) < lv_obj_get_y(focus_control_hint));
    focus_wheel_snap(0, 1);
    advance(5);
    assert(lv_obj_get_style_y(focus_minute_digits, LV_PART_MAIN) != 196);
    advance(145);
    assert(lv_obj_get_style_y(focus_minute_digits, LV_PART_MAIN) == 196);
    drag(300, 245, 240, 15); expect_page(0);
    drag(420, 40, 240, 25); expect_page(1);
    /* Three-page requirement supersedes the old two-page endstop: the
     * second left swipe is now valid page 2, not a clamped page 1. */
    drag(300, 40, 240, 20); expect_page(2);
    drag(40, 420, 240, 25); expect_page(1);
    drag(100, 380, 240, 20); expect_page(0);
    drag(330, 30, 230, 16); /* Swiping over the timer must not activate it. */
    assert(!focus_timer.running);
    int page = lv_obj_get_scroll_x(focus_pager) / 480;
    assert(page >= 0 && page <= 2); expect_page(page);
    focus_go_page(2, true); advance(1200); expect_page(2);
    sim_bili = (FocusBiliStatus){};
    sim_bili.state = 2; sim_bili.ready = true;
    snprintf(sim_bili.transport, sizeof(sim_bili.transport), "READY");
    focus_bili_refresh();
    assert(strcmp(lv_label_get_text(focus_bili_state), "READY") == 0);
    assert(strcmp(lv_label_get_text(focus_bili_timer), "TALK") == 0);
    assert(lv_obj_get_style_text_font(focus_bili_timer, LV_PART_MAIN)->line_height < 80);
    sim_bili.state = 4; sim_bili.recording_ms = 1234;
    focus_bili_refresh();
    assert(strcmp(lv_label_get_text(focus_bili_timer), "01.2") == 0);
    assert(lv_obj_get_style_text_font(focus_bili_timer, LV_PART_MAIN)->line_height < 80);
    sim_bili.state = 9; sim_bili.result = 1;
    snprintf(sim_bili.summary, sizeof(sim_bili.summary), "完成");
    focus_bili_refresh();
    assert(strcmp(lv_label_get_text(focus_bili_state), "DONE") == 0);
    sim_bili.result = 3;
    snprintf(sim_bili.summary, sizeof(sim_bili.summary), "没有可用的播放器状态，请先播放视频");
    focus_bili_refresh();
    assert(strcmp(lv_label_get_text(focus_bili_result), "NO PLAYER / PLAY A VIDEO") == 0);
    focus_go_page(0, false); advance(100); expect_page(0);
    focus_go_page(0, false); advance(100);
    focus_action(7);
    drag_xy(160, 340, 160, 268, 10); /* Instruction area is part of minute wheel. */
    assert(focus_timer.duration_ms >= 27u * 60000u && focus_timer.duration_ms <= 28u * 60000u);
    uint32_t after_minutes = focus_timer.duration_ms;
    advance(300);
    drag_xy(320, 250, 320, 202, 8);
    assert(focus_timer.duration_ms > after_minutes && focus_timer.duration_ms <= after_minutes + 2000u);
    assert(!lv_obj_has_flag(focus_wheel_prev[0], LV_OBJ_FLAG_HIDDEN));
    pointer(320, 230, true, 80); pointer(320, 230, false, 100);
    assert(focus_timer.running);
    assert(strcmp(lv_label_get_text(focus_state), "FOCUSING") == 0);
    uint32_t running_duration = focus_timer.duration_ms;
    drag_xy(160, 250, 160, 202, 8); /* Editing is locked while running. */
    assert(focus_timer.duration_ms == running_duration);
    advance(1000);
    assert(focus_timer.remaining_ms < focus_timer.duration_ms);
    focus_go_page(1, true); advance(1200); expect_page(1);
    assert(!lv_obj_has_flag(focus_arcs[1], LV_OBJ_FLAG_HIDDEN));
    unsigned remaining = focus_timer.remaining_ms;
    focus_set_connected(false); advance(500);
    assert(focus_timer.running && focus_timer.remaining_ms < remaining);
    focus_action(0); assert(media_count == 0);
    focus_set_connected(true); focus_action(0);
    assert(media_count == 1 && last_media == 0xCD);
    focus_action(-1); assert(last_media == 0xB6);
    focus_action(1); assert(last_media == 0xB5);
    focus_action(5); focus_action(0); assert(last_media == 0xE2);
    focus_action(-1); assert(last_media == 0xEA);
    focus_action(1); assert(last_media == 0xE9);
    focus_go_page(0, true); advance(70);
    pointer(200, 240, true, 80); pointer(210, 240, false, 1400);
    int x = lv_obj_get_scroll_x(focus_pager);
    assert(x == 0 || x == 480);
    focus_go_page(0, false); advance(100);
    pointer(350, 240, true, 40); pointer(270, 240, true, 40);
    pointer(170, 240, true, 40); pointer(90, 240, false, 1400);
    expect_page(1);
    focus_go_page(0, false); advance(100);
    focus_action(7);
    assert(strcmp(lv_label_get_text(focus_state), "FOCUS") == 0);
    assert(!focus_timer.running && focus_timer.remaining_ms == running_duration);
    focus_toggle(&focus_timer, sim_tick); advance(500);
    focus_toggle(&focus_timer, sim_tick);
    advance(100);
    assert(strcmp(lv_label_get_text(focus_state), "PAUSED") == 0);
    focus_toggle(&focus_timer, sim_tick);
    advance(100);
    focus_action(8);
    assert(focus_timer.running && focus_timer.remaining_ms == running_duration);
    focus_action(9); assert(!lv_obj_has_flag(focus_preset_overlay, LV_OBJ_FLAG_HIDDEN));
    focus_action(10); focus_action(11);
    assert(focus_timer.duration_ms == 15u * 60000u && !focus_timer.running);
    assert(lv_obj_has_flag(focus_preset_overlay, LV_OBJ_FLAG_HIDDEN));
    focus_reset(&focus_timer); focus_timer.remaining_ms = 50;
    focus_timer.running = true; focus_timer.last_ms = sim_tick;
    advance(200);
    assert(focus_timer.finished && !focus_timer.running);
    assert(strcmp(lv_label_get_text(focus_state), "REST") == 0);
    assert(strcmp(lv_label_get_text(focus_minute_digits), "00") == 0);
    assert(strcmp(lv_label_get_text(focus_second_digits), "00") == 0);
    puts("LVGL: snap, timer wheels, edit lock, stop/restart, presets, disconnect, six HID actions PASS");
}
static void screenshot(const char *path) {
#if LVGL_VERSION_MAJOR >= 9
    lv_draw_buf_t *snap = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_ARGB8888);
    assert(snap && snap->data);
    SDL_Surface *s = SDL_CreateRGBSurfaceFrom(snap->data, snap->header.w, snap->header.h,
        32, snap->header.stride, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
    assert(s); assert(SDL_SaveBMP(s, path) == 0);
    SDL_FreeSurface(s); lv_draw_buf_destroy(snap);
#else
    lv_img_dsc_t *snap = lv_snapshot_take(lv_scr_act(), LV_IMG_CF_TRUE_COLOR);
    assert(snap && snap->data);
#if LV_COLOR_16_SWAP
    uint16_t *pixels = (uint16_t *)snap->data;
    for (unsigned i = 0; i < snap->header.w * snap->header.h; ++i)
        pixels[i] = (pixels[i] >> 8) | (pixels[i] << 8);
#endif
    SDL_Surface *s = SDL_CreateRGBSurfaceFrom((void *)snap->data, snap->header.w, snap->header.h,
        16, snap->header.w * 2, 0xF800, 0x07E0, 0x001F, 0);
    assert(s); assert(SDL_SaveBMP(s, path) == 0);
    SDL_FreeSurface(s); lv_snapshot_free(snap);
#endif
    printf("Screenshot: %s\n", path);
}

static int fb_count;
static char fb_last_rating[24];
static char fb_last_cmd[40];
static void sim_feedback(const char *command_id, const char *rating,
                         const char *dismissed_by, int touch_count, int countdown) {
    (void)dismissed_by; (void)touch_count; (void)countdown;
    fb_count++;
    snprintf(fb_last_rating, sizeof(fb_last_rating), "%s", rating);
    snprintf(fb_last_cmd, sizeof(fb_last_cmd), "%s", command_id);
}

static void feedback_tests(void) {
    puts("feedback: overlay state-machine tests start");
    focus_ui_set_feedback_sink(sim_feedback);
    focus_go_page(PAGE_BILI, false); advance(200);
    focus_feedback_show("11111111-1111-4111-8111-111111111111");
    assert(!lv_obj_has_flag(focus_feedback_overlay, LV_OBJ_FLAG_HIDDEN));
    assert(strcmp(lv_label_get_text(focus_feedback_count), "5s") == 0);
    lv_obj_send_event(focus_feedback_overlay, LV_EVENT_PRESSED, NULL);
    advance(3000);
    assert(!lv_obj_has_flag(focus_feedback_overlay, LV_OBJ_FLAG_HIDDEN));
    lv_obj_send_event(focus_feedback_overlay, LV_EVENT_RELEASED, NULL);
    advance(1000);
    assert(!lv_obj_has_flag(focus_feedback_overlay, LV_OBJ_FLAG_HIDDEN));
    advance(5000);
    assert(lv_obj_has_flag(focus_feedback_overlay, LV_OBJ_FLAG_HIDDEN));
    assert(strcmp(fb_last_rating, "unfeedback") == 0);
    assert(fb_count == 1);

    focus_feedback_show("22222222-2222-4222-8222-222222222222");
    focus_feedback_select(true);
    assert(strcmp(fb_last_rating, "good") == 0);
    advance(2100);
    assert(lv_obj_has_flag(focus_feedback_overlay, LV_OBJ_FLAG_HIDDEN));
    assert(fb_count == 2);
    /* Even after GOOD/BAD, a 30 s review badge remains. */
    assert(!lv_obj_has_flag(focus_feedback_badge, LV_OBJ_FLAG_HIDDEN));

    focus_feedback_show("33333333-3333-4333-8333-333333333333");
    focus_feedback_hide("unfeedback", "next_ptt");
    assert(lv_obj_has_flag(focus_feedback_overlay, LV_OBJ_FLAG_HIDDEN));
    assert(!lv_obj_has_flag(focus_feedback_badge, LV_OBJ_FLAG_HIDDEN));
    assert(fb_count == 3);
    lv_obj_send_event(focus_feedback_badge, LV_EVENT_CLICKED, NULL);
    assert(!lv_obj_has_flag(focus_feedback_overlay, LV_OBJ_FLAG_HIDDEN));
    focus_feedback_hide("unfeedback", "timeout");
    assert(fb_count == 4);

    focus_feedback_show("33333333-3333-4333-8333-333333333333");
    for (int i = 0; i < 3; ++i) {
        lv_obj_send_event(focus_feedback_overlay, LV_EVENT_PRESSED, NULL);
        lv_obj_send_event(focus_feedback_overlay, LV_EVENT_RELEASED, NULL);
    }
    /* The fourth touch must not reset the countdown. */
    lv_obj_send_event(focus_feedback_overlay, LV_EVENT_PRESSED, NULL);
    lv_obj_send_event(focus_feedback_overlay, LV_EVENT_RELEASED, NULL);
    advance(6000);
    assert(lv_obj_has_flag(focus_feedback_overlay, LV_OBJ_FLAG_HIDDEN));
    /* RESULT (9) decaying to READY (2) must NOT close the 5 s overlay. */
    sim_bili.state = 9; sim_bili.result = 1;
    snprintf(sim_bili.command_id, sizeof(sim_bili.command_id), "%s",
             "55555555-5555-4555-8555-555555555555");
    focus_refresh();
    assert(!lv_obj_has_flag(focus_feedback_overlay, LV_OBJ_FLAG_HIDDEN));
    sim_bili.state = 2; focus_refresh();
    assert(!lv_obj_has_flag(focus_feedback_overlay, LV_OBJ_FLAG_HIDDEN));
    /* FAILED also opens the feedback window. */
    sim_bili.state = 9; sim_bili.result = 3;
    snprintf(sim_bili.command_id, sizeof(sim_bili.command_id), "%s",
             "77777777-7777-4777-8777-777777777777");
    focus_refresh();
    assert(!lv_obj_has_flag(focus_feedback_overlay, LV_OBJ_FLAG_HIDDEN));
    /* A genuinely new command ID closes the old feedback and badge. */
    snprintf(sim_bili.command_id, sizeof(sim_bili.command_id), "%s",
             "66666666-6666-4666-8666-666666666666");
    sim_bili.state = 3; focus_refresh();
    assert(lv_obj_has_flag(focus_feedback_overlay, LV_OBJ_FLAG_HIDDEN));
    focus_feedback_hide("unfeedback", "timeout");
    focus_feedback_hide("unfeedback", "timeout");
    sim_bili = (FocusBiliStatus){};
    lv_obj_scroll_to_x(focus_pager, 0, LV_ANIM_OFF);
    focus_page = PAGE_FOCUS;
    advance(200);
    puts("feedback: overlay state-machine tests PASS");
}
int main(int argc, char **argv) {
    bool test = argc > 1 && strcmp(argv[1], "--test") == 0;
    bool capture = argc > 1 && strcmp(argv[1], "--screenshot") == 0;
    lv_init();
#if LVGL_VERSION_MAJOR >= 9
    if (test || capture) {
        lv_tick_set_cb(test_clock);
        lv_display_t *d = lv_display_create(480, 480);
        static uint8_t buffer[480 * 40 * 4];
        lv_display_set_buffers(d, buffer, NULL, sizeof(buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_display_set_flush_cb(d, flush);
        lv_indev_t *indev = lv_indev_create();
        lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(indev, read_pointer);
    } else {
        lv_sdl_window_create(480, 480);
        lv_sdl_window_set_title(lv_display_get_default(), "Desk-Mate / Focus + Mac");
        lv_sdl_mouse_create();
    }
#else
    if (!test && !capture) { fprintf(stderr, "Use LVGL 9 simulator for interactive mode.\n"); return 2; }
    static lv_color_t pixels[480 * 40];
    static lv_disp_draw_buf_t buf;
    static lv_disp_drv_t display;
    static lv_indev_drv_t input;
    lv_disp_draw_buf_init(&buf, pixels, NULL, 480 * 40);
    lv_disp_drv_init(&display);
    display.hor_res = display.ver_res = 480;
    display.draw_buf = &buf; display.flush_cb = flush;
    lv_disp_drv_register(&display);
    lv_indev_drv_init(&input); input.type = LV_INDEV_TYPE_POINTER; input.read_cb = read_pointer;
    lv_indev_drv_register(&input);
#endif
    focus_ui_build(lv_screen_active(), sim_media, sim_battery);
    focus_ui_set_bili_source(sim_read_bili);
#if LVGL_VERSION_MAJOR < 9
    lv_mem_monitor_t memory;
    lv_mem_monitor(&memory);
    printf("Hardware view LVGL heap: %u bytes\n", (unsigned)(memory.total_size - memory.free_size));
    assert(memory.total_size - memory.free_size < 48u * 1024u);
#endif
    assert(strcmp(lv_label_get_text(focus_battery), "85%") == 0);
    if (test) { run_tests(); feedback_tests(); return 0; }
    if (capture) {
        int page = argc > 2 ? atoi(argv[2]) : 0;
        if (argc > 4 && strcmp(argv[4], "connected") == 0) {
            focus_set_connected(true); focus_toggle(&focus_timer, sim_tick); advance(1000);
        }
        focus_go_page(page, false); advance(300);
        if (page == PAGE_BILI) {
            focus_feedback_show("44444444-4444-4444-8444-444444444444");
            advance(100);
        }
        screenshot(argc > 3 ? argv[3] : "/private/tmp/deskmate.bmp");
        return 0;
    }
    while (1) { lv_timer_handler(); SDL_Delay(5); }
}
