#include <Arduino.h>
#include <Wire.h>
#include "lvgl.h"
#include "bsp_lvgl_port.h"
#include "./src/port_bsp/i2c_bsp.h"
#include "./src/port_bsp/axp2101_bsp.h"

#define DESKMATE_UI_IMPL 1
#include "ui_pages.h"

I2cMasterBus I2cMasterBus_(GPIO_NUM_7, GPIO_NUM_8, I2C_NUM_0);

void setup() {
  delay(2000);
  Custom_PmicPortInit(&I2cMasterBus_, 0x34);
  einstein_img_init();

  Wire.begin(7, 8);
  Wire.beginTransmission(0x34);
  Wire.write(0x27); Wire.write(0x03);
  Wire.endTransmission();

  pinMode(9, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(9), []{ esp_restart(); }, FALLING);

  bsp_lvgl_init(I2cMasterBus_);

  if (bsp_lvgl_lock(0)) {
    ui_build_all(lv_scr_act());
    bsp_lvgl_unlock();
  }
}

void loop() { delay(5); }
