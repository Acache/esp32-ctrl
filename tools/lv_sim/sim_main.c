/**
 * Desk-Mate LVGL 桌面模拟器 — SDL HAL 入口
 * UI 代码全部在 ui_pages.h，此文件只负责:
 *   1. SDL 窗口初始化 (sdl_hal_init)
 *   2. 调用 ui_build_all() 构建全部 6 页
 *   3. 渲染帧 + 截图
 *   4. 事件循环
 *
 * 编译: cp 到 lv_port_pc_vscode/src/main.c, 配合 CMake+SDL2 make
 */

#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "lvgl/lvgl.h"
#include <SDL.h>
#include "hal/hal.h"

/* 共享 UI 代码 */
#define DESKMATE_UI_IMPL 1
#include "/Users/acache/AI_PROJ/desk-mate/firmware/desk-mate-app/ui_pages.h"

/* ========== 截图保存 (BMP 24-bit) ========== */
static void sim_save_screenshot(const char *path) {
    lv_draw_buf_t *snap = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_ARGB8888);
    if (!snap || !snap->data) { printf("Snapshot failed\n"); return; }

    int w = snap->header.w, h = snap->header.h, st = snap->header.stride;
    uint32_t *px = (uint32_t *)snap->data;
    FILE *f = fopen(path, "wb");
    if (!f) { lv_draw_buf_destroy(snap); return; }

    int rs = (w * 3 + 3) & ~3;
    unsigned char hd[54] = {0};
    hd[0] = 'B'; hd[1] = 'M';
    hd[2] = 54 + rs * h; hd[10] = 54; hd[14] = 40;
    hd[18] = w; hd[19] = w >> 8; hd[22] = h; hd[23] = h >> 8;
    hd[26] = 1; hd[28] = 24;
    fwrite(hd, 1, 54, f);

    unsigned char *rw = malloc(rs);
    for (int y = h - 1; y >= 0; y--) {
        for (int x = 0; x < w; x++) {
            uint32_t p = px[y * (st / 4) + x];
            rw[x * 3]     = p & 0xff;
            rw[x * 3 + 1] = (p >> 8) & 0xff;
            rw[x * 3 + 2] = (p >> 16) & 0xff;
        }
        fwrite(rw, 1, rs, f);
    }
    free(rw); fclose(f);
    lv_draw_buf_destroy(snap);
    printf("OK %s (%dx%d)\n", path, w, h);
}

/* ========== MAIN ========== */
int main(int argc, char **argv) {
    (void)argc; (void)argv;
    lv_init();
    sdl_hal_init(480, 480);

    /* Build all UI (uses ui_pages.h) */
    lv_obj_t *scr = lv_scr_act();
    ui_build_all(scr);

    /* Render frames */
    for (int f = 0; f < 80; f++) {
        uint32_t s = lv_timer_handler();
        usleep((s == LV_NO_TIMER_READY ? LV_DEF_REFR_PERIOD : s) * 1000);
    }

    /* Screenshot support: LVGL_PAGE=0-5 to scroll before capture */
    const char *sp = getenv("LVGL_SCREENSHOT");
    if (!sp) sp = "/tmp/lvgl_firmware.bmp";

    const char *pg_env = getenv("LVGL_PAGE");
    if (pg_env && ui_scroll_cn) {
        int target = atoi(pg_env);
        if (target > 0 && target < 6) {
            lv_obj_scroll_to_x(ui_scroll_cn, target * 464, LV_ANIM_OFF);
            for (int f = 0; f < 20; f++) {
                uint32_t s = lv_timer_handler();
                usleep((s == LV_NO_TIMER_READY ? LV_DEF_REFR_PERIOD : s) * 1000);
            }
        }
    }

    sim_save_screenshot(sp);

    /* Event loop */
    while (1) {
        uint32_t s = lv_timer_handler();
        usleep((s == LV_NO_TIMER_READY ? LV_DEF_REFR_PERIOD : s) * 1000);
    }
    return 0;
}
