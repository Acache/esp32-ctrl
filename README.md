# Desk-Mate 🖥️🤖

基于 **ESP32-C6-Touch-AMOLED-2.16** 的桌面编程辅助助手。

## 硬件

- **主控:** ESP32-C6 (WiFi 6 + BLE 5)
- **屏幕:** 2.16 寸 AMOLED 480×480 触摸屏 (WaveShare)
- **供电:** 板载锂电池
- **分区:** no_ota (2MB APP)
- [官方文档](https://docs.waveshare.net/ESP32-C6-Touch-AMOLED-2.16)

## 功能

| 页面 | 功能 | 状态 |
|------|------|------|
| HOME | Einstein像素桌宠 + 番茄环绕 + 多provider额度 | ✅ |
| PLANS | 5个LLM套餐动态进度条 | ✅ |
| CONFIRM | ACP请求确认/取消 (大按钮) | ✅ |
| BLUETOOTH | 蓝牙设备扫描/连接/断开 | ✅ |
| SETTINGS | 番茄钟设置 + 音频 + 关于 | ✅ |
| VOICE | 麦克风 + 上移/下移发送到不同Agent | ✅ |

## 项目结构

```
desk-mate/
├── README.md
├── firmware/desk-mate-app/  ← 硬件固件（Arduino + LVGL + NimBLE + I2S）
│   ├── desk-mate-app.ino    ← 主固件 (924行)
│   ├── einstein_face.h      ← Einstein像素头像 (lv_canvas)
│   ├── jokes.h              ← 100句编程笑话
│   └── lv_conf.h            ← LVGL 硬件配置
├── spec/                    ← 设计规格
│   ├── deskmate-app-design.md
│   └── ble-protocol.md
├── docs/                    ← 设计稿
│   └── DeskMate_Design_v12.pdf
├── log/                     ← 经验日志
│   ├── learnings.md
│   └── troubleshooting.md
└── tools/                   ← 辅助工具
    ├── lv_sim/              ← 桌面模拟器 + README
    │   └── README.md        ← 模拟器vs固件区别说明
    ├── auto_checker.py
    ├── screenshot_viewer.py
    └── screenshot_wifi.py
```

## 两套代码

| | 模拟器 | 固件 |
|---|---|---|
| **位置** | `tools/lv_sim/sim_main.c` | `firmware/desk-mate-app/desk-mate-app.ino` |
| **用途** | 快速 UI 迭代 + 截图验证 | 烧录到硬件板子 |
| **编译** | CMake + SDL2 (Mac) | arduino-cli (ESP32-C6) |
| **蓝牙/语音等** | ❌ | ✅ |

详见 `tools/lv_sim/README.md`

## 编译烧录

```bash
cd firmware/desk-mate-app
arduino-cli compile --fqbn "esp32:esp32:esp32c6:PartitionScheme=no_ota"
arduino-cli upload  --fqbn "esp32:esp32:esp32c6:PartitionScheme=no_ota" --port /dev/cu.usbmodem*
```

## BLE 协议

参考 `spec/ble-protocol.md` — JSON over GATT, 双 Characteristic (TX Notify / RX Write)
