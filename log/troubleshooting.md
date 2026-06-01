# Desk-Mate Troubleshooting Guide

## 烧录相关

### esptool 连接不上
```
Failed to connect to ESP32-C6: No serial data received
```
**原因:** 板子运行的程序占用了 USB 控制器
**解决:**
1. 按住 **BOOT** 键不松
2. 按一下 **PWR** 键
3. 松开 BOOT
4. 重新烧录

### 板子烧录后卡死（屏幕定格、WiFi 不开、触摸无反应）
**症状:** 屏幕显示最后一帧画面，所有交互无反应
**原因:** 
- 8MB 分区表 → 板子无法正常启动
- 或 WiFi 软 AP 初始化在特定分区表下死锁
**解决:** 换回默认 4MB 分区表重新编译烧录

## 串口相关

### Serial.println() 无输出
**原因1:** USB 端口连接的是 USB-Serial-JTAG，不是 UART
**解决:** 无需解决，改用 WiFi 截图查看

## 编译相关

### text section exceeds available space
**原因:** APP 分区空间不足（4MB 分区只有 1.2MB APP 区）
**解决:**
- 关闭大字体（48→32）
- 关闭不需要的 LVGL 功能
- 或换分区表（但 8MB 有卡死风险）

### fatal error: lv_conf.h: No such file or directory
**原因:** LVGL 库需要 lv_conf.h 在 sketch 目录
**解决:** `cp path/to/lv_conf.h <sketch_dir>/`

### redefinition of 'setup' / 'loop'
**原因:** Arduino 编译时合并目录下所有 .ino，存在多个 .ino 文件
**解决:** 删除旧的 .ino 文件，只保留一个

## 硬件排查

### 屏幕不亮
1. 检查 AXP2101 初始化（屏幕电源由 ALDO3 控制）
2. 检查 DisplayPort_DispReset() 中的电源 toggle 时序
3. 检查 bsp_lvgl_init 是否调用了 spi_bus_initialize

### 触摸无反应
1. 确认 CST9220 I2C 地址为 0x5A
2. 检查 I2C 引脚：SCL=GPIO7, SDA=GPIO8
3. 检查触摸复位引脚 BSP_LCD_TOUCH_RST = GPIO11

## 固件相关（2026-05-30 新增）

### 编译错误：lv_font_montserrat_XX not declared
**现象**: 编译报错 `use of undeclared identifier 'lv_font_montserrat_12'`
**原因**: `lv_conf.h` 中对应字体未启用（默认 0）
**解决**: 在 sketch 目录的 `lv_conf.h` 中设置 `#define LV_FONT_MONTSERRAT_12 1`

### 编译错误：BSP_I2S_SCLK not declared
**现象**: 语音模块编译失败
**原因**: WaveShare 板子的 I2S 引脚宏未定义
**解决**: 在 `.ino` 文件头部手动添加：
```c
#define BSP_I2S_SCLK  GPIO_NUM_18
#define BSP_I2S_LCLK  GPIO_NUM_17
#define BSP_I2S_DSIN  GPIO_NUM_16
```

### lv_canvas 导致 OOM
**现象**: 烧录后屏幕黑屏或程序崩溃
**原因**: `LV_IMG_CF_TRUE_COLOR` 是 32-bit 色深，180×180 canvas = 129KB，超出 ESP32-C6 剩余 RAM
**解决**: 缩小 canvas 到 100×100 (40KB) 或改用 `LV_IMG_CF_TRUE_COLOR_16BIT`

### NimBLE API 编译错误
**现象**: `class 'NimBLEAdvertising' has no member named 'setScanResponse'`
**原因**: NimBLE v2.5.0 API 改名 `setScanResponseData`
**解决**: 删掉这行调用（广播基本 service UUID 已足够）

### ArduinoJson 版本选择
**现象**: 需要真正的 JSON 解析（替代 `string::find`）
**选择**: ArduinoJson v7.4.3（最新稳定版）
**安装**: `arduino-cli lib install ArduinoJson`
**引用**: `#include <ArduinoJson.h>`，使用 `JsonDocument` + `deserializeJson()`

### base64 编码
**现象**: 语音 PCM 数据之前用 hex 编码（`%04x`），效率低
**解决**: 手写标准 RFC 4648 base64 编码器（约 20 行 C），无需额外库
```c
static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
// 每3字节 → 4个base64字符
```
