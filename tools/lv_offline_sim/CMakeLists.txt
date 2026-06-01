cmake_minimum_required(VERSION 3.10)
project(lv_offline_sim C)

set(CMAKE_C_STANDARD 99)

set(LVGL_DIR ${CMAKE_SOURCE_DIR}/lvgl)
set(LVGL_SRC_DIR ${LVGL_DIR}/src)

file(GLOB_RECURSE LVGL_SOURCES ${LVGL_SRC_DIR}/*.c)
list(FILTER LVGL_SOURCES EXCLUDE REGEX "demos|examples|tests")
list(FILTER LVGL_SOURCES EXCLUDE REGEX "draw/swpmt|draw/renesas|draw/nxp|draw/stm32")
list(FILTER LVGL_SOURCES EXCLUDE REGEX "extra/libs/freetype|extra/libs/ffmpeg|extra/libs/qrcodegen|extra/libs/sjpg")
list(FILTER LVGL_SOURCES EXCLUDE REGEX "font/lv_font_simsun|font/lv_font_unscii")

add_executable(lv_sim main.c ${LVGL_SOURCES})

target_include_directories(lv_sim PRIVATE ${LVGL_DIR} ${LVGL_SRC_DIR})
