#ifndef EINSTEIN_FACE_H
#define EINSTEIN_FACE_H

#include "lvgl.h"

#define EF_SIZE 100

/* LVGL v8 API */
static lv_obj_t *einstein_create(lv_obj_t *parent, const char *img_path) {
    lv_obj_t *img = lv_img_create(parent);
    if (img_path) {
        lv_img_set_src(img, img_path);
    }
    lv_obj_set_size(img, EF_SIZE, EF_SIZE);
    return img;
}

#endif
