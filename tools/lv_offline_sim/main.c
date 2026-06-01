/*
 * LVGL 离线模拟器 - 表情包版
 * 模拟 desk-mate-emote.ino 的 UI 逻辑，渲染到 BMP
 *
 * 编译: cd build && python3 -m cmake .. && make && ./lv_sim
 * 输出: lvgl_screenshot.raw (RGB888)
 * 转换: python3 rgb888_to_bmp.py lvgl_screenshot.raw lvgl_screenshot.bmp 480 480
 */

#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HOR_RES 480
#define VER_RES 480

/* 显示 buffer */
static lv_color_t *disp_buf1 = NULL;
static lv_color_t *disp_buf2 = NULL;
static lv_disp_draw_buf_t draw_buf;
static lv_disp_drv_t disp_drv;

static void my_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    lv_disp_flush_ready(drv);
}

int main() {
    printf("=== LVGL Offline Simulator ===\n");
    lv_init();

    disp_buf1 = (lv_color_t *)malloc(HOR_RES * 50 * sizeof(lv_color_t));
    disp_buf2 = (lv_color_t *)malloc(HOR_RES * 50 * sizeof(lv_color_t));
    lv_disp_draw_buf_init(&draw_buf, disp_buf1, disp_buf2, HOR_RES * 50);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = HOR_RES; disp_drv.ver_res = VER_RES;
    disp_drv.draw_buf = &draw_buf; disp_drv.flush_cb = my_flush_cb;
    lv_disp_drv_register(&disp_drv);

    /* ===== 模拟 desk-mate-emote UI ===== */
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_make(0x1A, 0x1A, 0x2E), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, ":-)");
    lv_obj_set_style_text_color(label, lv_color_make(0x4E, 0xCA, 0xF5), 0);
    lv_obj_center(label);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_32, 0);

    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, "Click=anim Double=next");
    lv_obj_set_style_text_color(hint, lv_color_make(0x55, 0x55, 0x55), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -30);

    /* 触发 UI 渲染 */
    lv_timer_handler();

    /* 截图 - lv_snapshot 返回 RGB888 格式 */
    lv_obj_t *scr_act = lv_disp_get_scr_act(NULL);
    uint32_t snap_size = lv_snapshot_buf_size_needed(scr_act, LV_IMG_CF_TRUE_COLOR);
    uint8_t *snap_buf = (uint8_t *)malloc(snap_size);
    
    lv_img_dsc_t img;
    lv_res_t res = lv_snapshot_take_to_buf(scr_act, LV_IMG_CF_TRUE_COLOR, &img, snap_buf, snap_size);
    
    if (res == LV_RES_OK) {
        printf("Snapshot: %dx%d, %d bytes, cf=%d\n",
               img.header.w, img.header.h, snap_size, img.header.cf);
        
        /* 保存为 raw RGB888 */
        FILE *f = fopen("lvgl_screenshot.raw", "wb");
        if (f) {
            fwrite(img.data, 1, img.header.w * img.header.h * 3, f);
            fclose(f);
            printf("Raw RGB888 saved: lvgl_screenshot.raw\n");
        }
    } else {
        printf("Snapshot failed!\n");
    }
    
    free(snap_buf);
    lv_deinit();
    free(disp_buf1); free(disp_buf2);
    printf("Done.\n");
    return 0;
}