# Arduino & ESP32 官方文档速查

> 从 docs.arduino.cc 和 docs.espressif.com 扒下来的关键文档
> 按 Desk-Mate 项目的实际需要组织

---

## 1. Arduino CLI 命令速查

### 编译
```bash
arduino-cli compile --fqbn "esp32:esp32:esp32c6:PartitionScheme=no_ota" [sketch_path]
```

关键参数：
- `-b, --fqbn`        Fully Qualified Board Name
- `-u, --upload`      编译后自动烧录
- `-v, --verbose`     详细输出
- `--clean`           清除缓存重编
- `--board-options`   板级选项（如 PartitionScheme=no_ota）
- `--warnings`        警告级别：none, default, more, all

### 烧录
```bash
arduino-cli upload --fqbn "esp32:esp32:esp32c6:PartitionScheme=no_ota" --port /dev/cu.usbmodem1101 [sketch_path]
```
**注意：`upload` 不会自动编译，需要先 `compile`。**

### 库管理
```bash
# 安装
arduino-cli lib install ArduinoJson
arduino-cli lib install ArduinoJson@7.4.3    # 指定版本

# 搜索
arduino-cli lib search NimBLE

# 列出已安装
arduino-cli lib list

# 从 Git 安装
arduino-cli lib install --git-url https://github.com/h2zero/NimBLE-Arduino.git
```

### 板子/核心管理
```bash
arduino-cli board list                           # 列出已连接的板子
arduino-cli board listall esp32c6               # 搜索所有 C6 板子
arduino-cli core list                            # 已安装的平台
arduino-cli core update-index                    # 更新索引（首次必做）
```

---

## 2. ESP32 官方故障排除

来源：[docs.espressif.com/arduino-esp32](https://docs.espressif.com/projects/arduino-esp32/en/latest/troubleshooting.html)

### 烧录失败 "Timed out waiting for packet header"
**最可能的原因**：GPIO0 不在下载模式。
**解决步骤**（按优先级）：
1. 检查 USB 线（有些线只能充电不传数据）
2. 换 USB 口（直连电脑，避免 USB Hub）
3. 检查供电
4. TX/RX 引脚不要接任何东西
5. **按住 BOOT 键 → 按 RST → 松开 BOOT** （让 GPIO0 拉低进入下载模式）
6. 在 RST 和 GND 之间并联一个 10μF 电容

### 板子不被电脑识别
- 检查 USB 驱动：https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
- 换 USB 线
- 换 USB 口
- 检查供电

### WiFi 连接问题
```cpp
WiFi.setMinSecurity(WIFI_AUTH_WEP); // 降到 WEP
WiFi.setMinSecurity(WIFI_AUTH_WPA_PSK); // 降到 WPA
```

### ESP32-C6 特殊注意（USB-Serial/JTAG）
**ESP32-C6 使用内置 USB-Serial/JTAG**，不是 UART。
- `Serial` 对象可能没有输出 → 用 `USBSerial` 或 `Serial0`
- 烧录端口是 `/dev/cu.usbmodem*`

### 中国用户下载加速
使用极狐镜像：
```
https://jihulab.com/esp-mirror/espressif/arduino-esp32/-/raw/gh-pages/package_esp32_index_cn.json
```

---

## 3. ESP32 分区表

### 我们的板子 (ESP32-C6 WaveShare)
- **Flash:** 4MB（重要！不要相信 datasheet 上的"最高支持"，以实际芯片为准）
- **确认方法：** 烧录时 esptool 输出 `Configuring flash size...` 查看 `Flash will be erased from 0x00000000 to 0x...`

### arduino-esp32 3.3.8-cn 提供的 ESP32-C6 分区选项
| 分区名 | APP 大小 | 说明 |
|--------|---------|------|
| `default` | 1.2MB | 默认 4MB + SPIFFS |
| `default_8MB` | 3MB | ⚠️ 需要 8MB flash，**我们的板子不支持** |
| `no_ota` | 2MB | 无 OTA，最大的 APP 空间（4MB内） |
| `minimal` | 1.3MB | 最小 SPIFFS |
| `noota_3g` | 1MB | 3MB SPIFFS |

**我们用 `no_ota`：2MB APP + 2MB SPIFFS。当前固件 1.7MB/2MB (82%)。**

---

## 4. Arduino CLI 配置

### 初始化
```bash
arduino-cli config init
# → 写入 ~/.arduino15/arduino-cli.yaml
```

### 添加第三方板子 URL
在 `arduino-cli.yaml` 中：
```yaml
board_manager:
  additional_urls:
    - https://jihulab.com/esp-mirror/espressif/arduino-esp32/-/raw/gh-pages/package_esp32_index_cn.json
```

### 查看已安装板子包
```bash
arduino-cli core list
# esp32:esp32    3.3.8-cn    4.0.0-alpha1    esp32
```

---

## 5. 库规范参考（library.properties）

每个 Arduino 库需要一个 `library.properties`：
```ini
name=MyLibrary
version=1.0.0
author=Your Name
maintainer=Your Name <email>
sentence=A short description
paragraph=A longer description
category=Communication
url=https://github.com/...
architectures=esp32
depends=ArduinoJson
```

`depends` 字段支持库名或库名@版本号，安装时自动拉取依赖。

---

## 6. Desk-Mate 项目用到的库清单

| 库 | 版本 | 作用 |
|----|------|------|
| `lvgl` | 8.4.0 | UI 图形库 |
| `NimBLE-Arduino` | 2.5.0 | BLE 通信 |
| `ArduinoJson` | 7.4.3 | JSON 解析 |
| `XPowersLib` | 0.3.3 | AXP2101 电源管理 |
| `WiFi` | 3.3.8 | WiFi（内置） |
| `WebServer` | 3.3.8 | HTTP 服务（内置） |

---

## 7. 正确的编译烧录命令（Desk-Mate）

```bash
# 1. 确保板子已进入下载模式（按住 BOOT → 按 PWR → 松开 BOOT）

# 2. 编译
cd /Users/acache/AI_PROJ/desk-mate/firmware/desk-mate-app
arduino-cli compile --fqbn "esp32:esp32:esp32c6:PartitionScheme=no_ota"

# 3. 烧录
arduino-cli upload --fqbn "esp32:esp32:esp32c6:PartitionScheme=no_ota" \
    --port /dev/cu.usbmodem1101
```

失败时检查：
1. `/dev/cu.usbmodem1101` 是否存在（`ls /dev/cu.*`）
2. 板子是否卡死（按 BOOT+PWR 恢复）
3. 分区表是否正确（必须是 `no_ota`，不能用 `default_8MB`）

---

*Last updated: 2026-05-30*
*Sources: docs.arduino.cc, docs.espressif.com*


---

## 8. LVGL v8.4 Style API 速查

来源：[docs.lvgl.io/8.4/overview/style.html](https://docs.lvgl.io/8.4/overview/style.html)

### 核心概念
- `lv_style_t` 类似 CSS 的 class
- 一个 style 可以赋给多个 object
- 后添加的 style 优先级更高（级联）
- 部分属性（text color）可从父对象继承

### 状态 (States)
```c
LV_STATE_DEFAULT    (0x0000)  // 正常
LV_STATE_CHECKED    (0x0001)  // 选中
LV_STATE_FOCUSED    (0x0002)  // 聚焦
LV_STATE_PRESSED    (0x0020)  // 按下
LV_STATE_SCROLLED   (0x0040)  // 滚动中
LV_STATE_DISABLED   (0x0080)  // 禁用
```

组合：`LV_STATE_FOCUSED | LV_STATE_PRESSED`（0x0022）
优先级 = 状态的数值，数值越大的状态优先级越高。

### 级联示例
```c
lv_style_t style_base, style_red;
lv_obj_add_style(btn, &style_base, 0);
lv_obj_add_style(btn, &style_red, 0);  // 后添加的覆盖前者
```

### 我们项目常用
```c
lv_obj_set_style_bg_color(obj, color, LV_PART_MAIN | LV_STATE_DEFAULT);
lv_obj_set_style_arc_color(obj, color, LV_PART_INDICATOR);
lv_obj_set_style_opa(obj, LV_OPA_TRANSP, LV_PART_KNOB);  // 隐藏 knob
lv_obj_set_style_radius(obj, number, 0);
lv_obj_set_style_text_font(obj, &lv_font_montserrat_12, 0);
```


## 9. LVGL v8.4 Event API 速查

来源：[docs.lvgl.io/8.4/overview/event.html](https://docs.lvgl.io/8.4/overview/event.html)

### 触摸事件
```c
LV_EVENT_PRESSED              // 按下
LV_EVENT_PRESSING             // 按住中（持续触发）
LV_EVENT_PRESS_LOST           // 按住后滑出对象
LV_EVENT_SHORT_CLICKED        // 短按（不被滚动打断）
LV_EVENT_LONG_PRESSED         // 长按 ≥ long_press_time
LV_EVENT_LONG_PRESSED_REPEAT  // 长按后持续重复
LV_EVENT_CLICKED              // 点击（不限长短按，不被滚动打断）
LV_EVENT_RELEASED             // 只要松手就触发（不管滚不滚动）
```

### 滚动事件
```c
LV_EVENT_SCROLL_BEGIN  // 开始滚动
LV_EVENT_SCROLL_END    // 滚动结束 ← 我们用这个更新页指示器
LV_EVENT_SCROLL        // 滚动中
```

### 手势
```c
LV_EVENT_GESTURE       // 手势检测
lv_indev_get_gesture_dir(lv_indev_get_act())  // 获取方向
```

### 事件绑定
```c
lv_obj_add_event_cb(obj, callback, LV_EVENT_CLICKED, user_data);
// user_data 在回调中通过 lv_event_get_user_data(e) 获取

// 同一个对象可以绑定多个事件
lv_obj_add_event_cb(obj, cb1, LV_EVENT_CLICKED, NULL);
lv_obj_add_event_cb(obj, cb2, LV_EVENT_LONG_PRESSED, NULL);
lv_obj_add_event_cb(obj, cb3, LV_EVENT_ALL, NULL);  // 监听全部
```

### 我们项目用法
```c
// 桌宠: click → 切笑话, long press → 吐槽
lv_obj_add_event_cb(pet, pet_tap_handler, LV_EVENT_CLICKED, joke_label);
lv_obj_add_event_cb(pet, pet_tap_handler, LV_EVENT_LONG_PRESSED, joke_label);

// 滑屏: scroll end → 更新圆点
lv_obj_add_event_cb(scroll_cn, on_scroll_end, LV_EVENT_SCROLL_END, NULL);
```


## 10. ESP32 I2S API 速查

来源：[docs.espressif.com/arduino-esp32 API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/i2s.html)

### I2S 总线三线
- **BCLK** (bit clock) → `sck` / `bck_io_num`
- **WS** (word select / LRCLK) → `ws` / `ws_io_num`
- **DATA** → `din`(输入) / `dout`(输出)

### 我们的配置 (16kHz 语音录制)
```c
i2s_config_t cfg = {
    .mode = I2S_MODE_MASTER | I2S_MODE_RX,    // 主模式 + 接收
    .sample_rate = 16000,                       // 16kHz
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,  // 单声道
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .dma_buf_count = 4,
    .dma_buf_len = 800,  // 50ms @ 16kHz mono 16bit = 1600 bytes → /2
};

i2s_pin_config_t pin = {
    .bck_io_num = GPIO_NUM_18,
    .ws_io_num = GPIO_NUM_17,
    .data_in_num = GPIO_NUM_16,
    .data_out_num = I2S_PIN_NO_CHANGE,
};

i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
i2s_set_pin(I2S_NUM_0, &pin);
```

### 读数据
```c
size_t bytes_read;
i2s_read(I2S_NUM_0, buffer, buf_size, &bytes_read, pdMS_TO_TICKS(100));
```

### 注意事项
- ESP32 原始版 (HW v1) 8-bit 单声道有硬件 bug → 库已自动修复
- ESP32-C6 使用 USB-Serial/JTAG 不是 UART → `Serial` 可能没输出
- PDM 麦克风需要 `I2S_MODE_PDM_RX`，仅 I2S0 支持


## 11. NimBLE vs ESP-BLE 选择

来源：[h2zero/NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino)

**我们选择 NimBLE 的原因：**
- **内存更小**：NimBLE 栈 ~80KB vs ESP-BLE ~200KB
- **ESP32-C6 官方推荐**
- **API 更简洁**

### NimBLE 创建服务（我们的项目）
```cpp
NimBLEServer *server = NimBLEDevice::createServer();
NimBLEService *svc = server->createService("0000d1ab-...");
tx_char = svc->createCharacteristic("...-0001-...", NIMBLE_PROPERTY::NOTIFY);
rx_char = svc->createCharacteristic("...-0002-...", NIMBLE_PROPERTY::WRITE);
rx_char->setCallbacks(new MyCallback());
svc->start();
```

### BLE Scan 示例
```cpp
BLEScan *scan = BLEDevice::getScan();
scan->setActiveScan(true);
scan->setInterval(100);
scan->setWindow(99);
BLEScanResults results = scan->start(5, false);
```

### 重要！
**ESP-BLE 和 NimBLE 不能同时使用**。ESP-BLE 的 `Bluedroid` 栈和 NimBLE 互斥。
在 `arduino-cli` 中，选择 NimBLE 就是安装 NimBLE-Arduino 库，不使用 ESP32 内置的 BLE 库。


---

*Last updated: 2026-05-30 23:45*
*Sources: docs.lvgl.io, docs.espressif.com, github.com/h2zero/NimBLE-Arduino*
