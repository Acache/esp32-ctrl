# LVGL 桌面模拟器

## 用途

在 Mac 上用 SDL 窗口运行 Desk-Mate UI，无需硬件。用于快速迭代 UI 设计、截图验证、视觉回归测试。

## 文件

| 文件 | 说明 |
|------|------|
| `sim_main.c` | 模拟器入口，包含所有6页UI代码（纯视觉） |
| `sim_hal.c` | SDL 窗口初始化 (`sdl_hal_init`) |
| `sim_hal.h` | HAL 头文件 |
| `lv_conf.h` | 模拟器专用 LVGL 配置 |

## 编译

依赖 LVGL + SDL2，构建目录在 `/tmp/lv_sim_build/lv_port_pc_vscode/`：

```bash
cp sim_main.c /tmp/lv_sim_build/lv_port_pc_vscode/src/main.c
cd /tmp/lv_sim_build/lv_port_pc_vscode/build && make -j4
```

## 截图

```bash
# 截取首页
LVGL_SCREENSHOT=/tmp/page0.bmp /tmp/lv_sim_build/lv_port_pc_vscode/bin/main

# 截取指定页 (0-5)
LVGL_PAGE=2 LVGL_SCREENSHOT=/tmp/page2.bmp /tmp/lv_sim_build/lv_port_pc_vscode/bin/main
```

---

## 模拟器 vs 硬件固件 关键区别

| | 模拟器 (`sim_main.c`) | 硬件固件 (`desk-mate-app.ino`) |
|---|---|---|
| **窗口驱动** | `sdl_hal_init(480,480)` → SDL 窗口 | `bsp_lvgl_init(I2cMasterBus)` → AMOLED 屏幕 |
| **触摸输入** | SDL 鼠标事件 | CST9220 I2C 触摸屏 |
| **蓝牙** | ❌ 无 | ✅ NimBLE GATT Server |
| **语音** | ❌ 无 | ✅ I2S 麦克风 + base64 编码 |
| **番茄钟** | ❌ 静态硬编码 | ✅ lv_timer 真实倒计时 |
| **Token** | ❌ 硬编码假数据 | ✅ BLE JSON 动态更新 |
| **ACP** | ❌ 硬编码 | ✅ BLE 请求→按钮响应 |
| **笑话** | 5句写死 | 100句 + BLE `pet_say` 远程推送 |
| **UI代码** | 500行纯UI | 924行（UI + 业务逻辑） |
| **编译** | CMake + gcc | arduino-cli (esp32:esp32c6:PartitionScheme=no_ota) |

## 工作流

```
1. 改 UI 设计 → 模拟器快速预览 (sim_main.c)
2. 确认效果 → 截图 + MiniMax 视觉验证
3. UI 通过 → 把 UI 布局代码移植到固件 (desk-mate-app.ino)
4. 加业务逻辑 → 蓝牙/语音/番茄钟
5. 编译 + 烧录 → 硬件验证
```

## 自动截图验证脚本

```bash
# 截所有6页并分析
for pg in 0 1 2 3 4 5; do
  LVGL_PAGE=$pg LVGL_SCREENSHOT=/tmp/page$pg.bmp ./bin/main
  python3 -c "from PIL import Image; Image.open('/tmp/page$pg.bmp').save('/tmp/page$pg.png')"
done
# 然后用 MiniMax understand_image 逐图分析
```
