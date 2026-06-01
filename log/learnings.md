# Desk-Mate 开发经验 & 避坑指南

## 黄金法则

### 分区表不要换！
**ESP32-C6 的 arduino-esp32 中文魔改版 (3.3.8-cn) 在 8MB 分区表下会导致板子卡死。**
- 默认 4MB 分区（PartitionScheme=default）→ 一切正常
- 8MB 分区（PartitionScheme=default_8MB）→ 板子完全无响应
- 不要试图换分区表，除非确认该版本的库已修复此问题

### 4MB 分区的空间限制
- APP 区只有 **1.2MB**
- LVGL 库本体 ~450KB，WiFi+WebServer ~650KB
- 表情包功能（无 WiFi）~600KB（余量 600KB）
- 表情包+WiFi+32号字体 ~1.3MB → **99% 已满**
- 不能再加任何大功能

### 如果板子卡死了怎么办
1. 按住 **BOOT** 键 → 按一下 **PWR** → 松开 BOOT
2. 烧录一个能工作的程序（如最简版 minimal_test）
3. 之后直接烧录新程序即可，不用再按 BOOT

## 编译命令备忘

```bash
# 当前固件（no_ota 分区，2MB APP）
cd firmware/desk-mate-app
arduino-cli compile --fqbn "esp32:esp32:esp32c6:PartitionScheme=no_ota"

# 烧录
arduino-cli upload --fqbn "esp32:esp32:esp32c6:PartitionScheme=no_ota" --port /dev/cu.usbmodem1101

# ⚠️ 板子flash只有4MB，禁止用 default_8MB 分区！
```

## 工程文件结构

```
desk-mate/
├── firmware/desk-mate-app/  ← 硬件固件（烧录到板子）
│   ├── desk-mate-app.ino    ← 主固件 924行
│   ├── einstein_face.h      ← Einstein头像 (lv_canvas)
│   ├── jokes.h              ← 100句笑话
│   └── lv_conf.h            ← LVGL 硬件配置
├── tools/lv_sim/            ← 桌面模拟器（SDK + LVGL）
│   ├── sim_main.c           ← 模拟器UI代码 500行
│   ├── sim_hal.c/h          ← SDL窗口
│   └── README.md            ← 模拟器vs固件区别
├── spec/                    ← 设计规格
│   ├── deskmate-app-design.md
│   └── ble-protocol.md
├── docs/                    ← 文档+设计稿
├── log/                     ← 经验日志 + 踩坑记录
└── _waveshare_examples/     ← 官方示例（只读参考）
```

## lv_conf.h 必备配置

```c
#define LV_USE_SNAPSHOT       1   // 截图功能必需
#define LV_FONT_MONTSERRAT_32 1   // 大号文字
#define LV_FONT_MONTSERRAT_48 1   // 超大号（慎用，~50KB）
#define LV_USE_ANIMIMG        1   // 动画图片
#define LV_USE_IMG            1   // 图片显示
```

## LVGL 设计注意事项

### 字体规则
- LVGL 的 Montserrat 字体**不包含 Unicode 特殊字符**
- `▼ ▲ 🎤 ●` 等图标全部渲染为方块 → 改用纯 ASCII: `v ^ >`
- `²` 等上标字符也不支持 → 改为 `^2`
- 中文用思源黑体 CJK 字体但只含 1338 字符 → 不够用 → 界面全部英文

### LVGL arc 组件
- `lv_arc` 默认有**旋钮 (KNOB)**，需隐藏：
  `lv_obj_set_style_opa(a, LV_OPA_TRANSP, LV_PART_KNOB)`
- 多 arc 叠加做环图时，`MAIN` 部分和 `INDICATOR` 的颜色分别控制
- 不要用 `ring_bg`+`ring_seg` 分开创建 → **两个 arc 叠层会冲突**
- 正确做法：一个 arc 设 `bg_angles(0,360)` 做完整环，前景部分设 `angles(s,e)`

### 内存 vs Flash
- ESP32-C6 SRAM 512KB，全局变量从 BSS 段扣
- `lv_canvas` 在 `LV_IMG_CF_TRUE_COLOR`(32-bit) 下：大小=W×H×4 字节
  - 180×180×4=129KB → **OOM**
  - 100×100×4=40KB → OK
- no_ota 分区 2MB APP，flash 82%=1.7MB 仍然安全

### 固件编译踩坑
| 错误 | 原因 | 修复 |
|------|------|------|
| `fatal: lv_conf.h not found` | Arduino 编译需要 lv_conf.h 在 sketch 目录 | cp 过去 |
| `lv_font_montserrat_12 not declared` | lv_conf.h 默认关闭 | 手动开启 `#define LV_FONT_MONTSERRAT_12 1` |
| `setScanResponse no member` | NimBLE v2.5 API 改名 | 删掉这行或用 `setScanResponseData` |
| `BSP_I2S_SCLK not declared` | WaveShare 板子 I2S 引脚未定义 | 在源文件头部 `#define BSP_I2S_SCLK GPIO_NUM_18` 等 |

### 烧录流程
```bash
# 1. 按住 BOOT → 按一下 PWR → 松开 BOOT （板子卡死时）
# 2. 编译
cd firmware/desk-mate-app
arduino-cli compile --fqbn "esp32:esp32:esp32c6:PartitionScheme=no_ota"
# 3. 烧录
arduino-cli upload --fqbn "esp32:esp32:esp32c6:PartitionScheme=no_ota" --port /dev/cu.usbmodem1101
```

### 两套代码管理
- 模拟器代码 `tools/lv_sim/` 和固件代码 `firmware/desk-mate-app/` 分开维护
- UI 布局先在模拟器调试，再加业务逻辑移植到固件
- 模拟器没有 BLE/I2S/触摸硬件，只有 SDL 窗口

## 问题排查步骤

如果功能不正常，按此顺序排查：
1. 烧 minimal_test（最简版）→ 确认硬件和 bsp_lvgl_init 正常
2. 加 WiFi → 确认 WiFi 初始化不卡死
3. 加动画 → 确认 lv_anim 在局部变量上工作
4. 加触摸事件 → 确认 LV_EVENT_CLICKED 正常

每次只加一个变量，不要一次加多个功能。

## 2026-05-29 LVGL 模拟器调研

### LVGL PC 模拟器（官方）可工作
- `lv_port_pc_vscode` + CMake + SDL2 可以在 Mac 上编译运行
- 编译产物: `/tmp/lv_sim_build/lv_port_pc_vscode/bin/main`
- 分辨率 320x480（默认），需要改成 480x480

### LVGL v8 颜色格式关键点
```
lv_color_t = uint16_t = RGB565
lv_color_hex(0x1A1A2E) → RGB565: 0x18a5
大端存储: [0x18, 0xa5]
小端存储: [0xa5, 0x18]

BMP 小端系统: RGB565 要用 BGR565 读取
```

### MiniMax Vision API
- 内置工具: `MiniMax-Token-Plan-MCP__understand_image`
- 直接调用 MiniMax M2.7 的视觉能力
- 只需要提供本地 PNG/JPG 文件路径

### brew 超频解决方案
- 必须同时设置 4 个环境变量 + `HOMEBREW_API_DOMAIN`
- 中科大镜像: `https://mirrors.ustc.edu.cn/homebrew-bottles`
- 配置文件: `~/.zshrc`

### 问题：离线模拟器 lv_snapshot 拿不到正确渲染结果
- `lv_snapshot` 在没有真正显示上下文时，拿到的 buffer 内容异常
- 正确做法：用 PC 模拟器 + screencapture 获取渲染后的屏幕

### 2026-05-30 新增：PC 模拟器截图方案要点

#### SDL + LVGL 截图方案（已验证可用）
1. `lv_conf.h`：`LV_SDL_ACCELERATED = 0`, `LV_USE_SNAPSHOT = 1`
2. 在 main.c 中渲染 60 帧后调用 `lv_snapshot_take(scr, LV_COLOR_FORMAT_ARGB8888)`
3. **关键：ARGB8888 在内存中的字节序**（小端系统）
   - 内存排列：`[B, G, R, A]`（byte0=B, byte1=G, byte2=R, byte3=A）
   - 所以从 `uint32_t px` 中提取时：
     - `px & 0xff` → B
     - `(px >> 8) & 0xff` → G
     - `(px >> 16) & 0xff` → R
4. 输出 BMP 时要 BGR 顺序（BMP 标准），且 bottom-up 行序

#### Screencapture 在 SDL 加速模式下不可用
- `LV_SDL_ACCELERATED = 1` 时 SDL 使用 Metal 层，`screencapture` 截全屏全黑
- 必须关硬件加速或用 snapshot/RenderReadPixels 方式

#### 替代失效方案记录
- `lv_drivers/sdl/sdl.c` 里的 `monitor.tft_fb` 没被实际使用（LVGL 用的是内置 `lv_sdl_window` 驱动）
- `screencapture -l <window_id>` 需要 Accessibility 权限
- `SDL_RenderReadPixels` 有 GL context 问题


## 遇到问题的第一原则

**务必先去搜索查阅官方文档！**
- LVGL 问题 → https://docs.lvgl.io/8.4/
- ESP32 Arduino API → https://docs.espressif.com/projects/arduino-esp32/en/latest/api/
- Arduino CLI → https://docs.arduino.cc/arduino-cli/
- NimBLE → https://github.com/h2zero/NimBLE-Arduino
- 项目知识库汇总：`docs/arduino_knowledge_base.md`

**不要凭记忆猜 API、不要自己发明 API 名称**（如 `setScanResponse` vs `setScanResponseData`）。
