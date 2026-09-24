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
static void expect_page(int page) {
    int x = lv_obj_get_scroll_x(focus_pager);
    if (x != page * 480) fprintf(stderr, "Expected page %d at %d, got %d\n", page, page * 480, x);
    assert(x == page * 480);
    assert(focus_page == page);
}
static void run_tests(void) {
    advance(100); expect_page(0);
    drag(300, 245, 240, 15); expect_page(0);
    drag(420, 40, 240, 25); expect_page(1);
    drag(300, 40, 240, 20); expect_page(1);
    drag(40, 420, 240, 25); expect_page(0);
    drag(100, 380, 240, 20); expect_page(0);
    drag(242, 8, 368, 12); /* Swiping Play must not activate it. */
    assert(!focus_timer.running);
    int page = lv_obj_get_scroll_x(focus_pager) / 480;
    assert(page == 0 || page == 1); expect_page(page);
    focus_go_page(0, false); advance(100);
    pointer(240, 368, true, 80); pointer(240, 368, false, 100);
    assert(focus_timer.running);
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
    focus_reset(&focus_timer);
    pointer(388, 368, true, 2500); pointer(388, 368, false, 100);
    assert(focus_timer.duration_ms >= 35u * 60000u); /* accelerated hold */
    pointer(240, 447, true, 80); pointer(240, 447, false, 100);
    assert(focus_timer.duration_ms == 15u * 60000u); /* direct preset */
    focus_reset(&focus_timer); focus_timer.remaining_ms = 50;
    focus_timer.running = true; focus_timer.last_ms = sim_tick;
    advance(200);
    assert(focus_timer.finished && !focus_timer.running);
    assert(strcmp(lv_label_get_text(focus_digits), "00:00") == 0);
    puts("LVGL: snap, edges, interrupted animation, flick, drag-on-button, timer, disconnect, six HID actions PASS");
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
#if LVGL_VERSION_MAJOR < 9
    lv_mem_monitor_t memory;
    lv_mem_monitor(&memory);
    printf("Hardware view LVGL heap: %u bytes\n", (unsigned)(memory.total_size - memory.free_size));
    assert(memory.total_size - memory.free_size < 48u * 1024u);
#endif
    assert(strcmp(lv_label_get_text(focus_battery), "85%") == 0);
    if (test) { run_tests(); return 0; }
    if (capture) {
        int page = argc > 2 ? atoi(argv[2]) : 0;
        if (argc > 4 && strcmp(argv[4], "connected") == 0) {
            focus_set_connected(true); focus_toggle(&focus_timer, sim_tick); advance(1000);
        }
        focus_go_page(page, false); advance(300);
        screenshot(argc > 3 ? argv[3] : "/private/tmp/deskmate.bmp");
        return 0;
    }
    while (1) { lv_timer_handler(); SDL_Delay(5); }
}
