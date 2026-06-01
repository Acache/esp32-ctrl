# Desk-Mate 屏幕远程查看方案

## 需求
开发时能直接看到 ESP32-C6 开发板的屏幕状态，无需人工反馈。

## 约束
- ESP32-C6: 512KB SRAM (无 PSRAM)
- 屏幕: 480x480 RGB565 = 460KB (一帧)
- 不可用 lv_snapshot 全屏缓冲 (内存不够)

## 方案对比

### A. lv_snapshot (❌ 不可行)
- 需要 460KB 缓冲区，ESP32-C6 只有 512KB SRAM
- 系统+WiFi+LVGL 已占用约 200KB+，剩余约 300KB
- → OOM

### B. 串口逐行传输 (✅ 可行)
- LVGL draw buffer 只有 50 行 (480x50x2 = 48KB)
- 在 flush callback 中直接截获数据，不额外分配大内存
- PC 端通过串口接收 BMP 数据
- 缺点：只能截获 flush 中的部分行，不是全屏瞬时快照
- 变通：强制触发全屏重绘 → 收集完整的 flush 序列

### C. WiFi HTTP + 分块收集 (推荐 ⭐)
- 在 flush callback 中收集像素到行缓存
- 通过 WiFi HTTP 提供 BMP 下载
- 浏览器可直接查看
- 需要修改 bsp_lvgl_port.cpp 的 flush callback

### D. 物理方案 (备用)
- 手机摄像头拍屏幕
- 需人工介入

## 选定方案: C

### 实现思路
1. 在 `my_disp_flush` 中，除了正常 flush 外，将 `color_p` 数据复制到行缓存
2. 用行号索引标记每一行
3. 当 HTTP 请求 /screenshot 时：
   a. 触发 lv_invalidate_area 强制全屏重绘
   b. 在 flush 过程中收集所有行
   c. 收集完毕后生成 BMP 响应

### 更简单的实现
使用一个 480x480x2 = 460KB 的外部 SPI RAM？
ESP32-C6 官方不支持，所以还是不行。

### 最终方案：串口 + PC 脚本 (方案 B)
最简单可靠，不需要 WiFi 配置：
1. 固件：在 LVGL flush callback 中，每次把输出的行缓存下来
2. 当收到串口 'S' 命令时，触发全屏重绘，等收集完所有行后发送 BMP
3. PC 端：Python 脚本发送 'S'，接收 BMP 并显示/保存

### 或者更简洁: 直接修改示例代码

在最简单版本中, 我们直接在 08_LVGL_V8_Test 示例上:

1. 加 Serial 监听
2. 收到 'S' 后, 用 lv_invalidate_area(lv_scr_act()) 强制重绘
3. 在 flush callback 中判断是否在截图模式, 收集行数据
4. 收集满 480 行后发送 BMP

但这里的问题是 flush 发生在 LVGL task 里, 需要线程安全。

## 最终决策: WiFi AP + WebServer (独立task)

不用修改 LVGL 的 flush callback, 而是:

1. 在 setup 中启用 WiFi AP (DeskMate-XXXX)
2. 创建 WebServer, 提供 /screenshot 端点
3. 截图处理方式: 在 handler 里临时分配一个 480x48KB 的 buffer (只收集50行)
   分4次收集, 每次收到 flush 后存起来。但这样太复杂。

**最务实的方案**: 
就用 `lv_snapshot` 但目标不是全屏 → 先申请能申请到的最大 buffer (~200KB)
→ 截图对象的一部分 → 分块拼接。

**算了, 别绕了。最佳方案:**

### 最终实现 ⭐

在现有代码基础上, 做两件事:

1. **启用 LV_USE_SNAPSHOT = 1** 在 lv_conf.h
2. **截图时不截全屏, 只截部分区域 (例如 120x480)** 
   - 需要 120x480x2 = 115KB 缓冲区 √ 这个够
   - 分 4 次截图: 4 个 120x480 区域拼成完整 480x480
   - PC 端拼图

或者更聪明的办法:

3. **不截图, 直接让 canvas 输出到 WebSocket stream**
   - LVGL widget 的内容可以直接序列化
   - 但像素级别还是不行

**终极简化方案: 串口 + 命令**

保持现有代码几乎不动, 只在 loop 里加一行:

```cpp
if (Serial.available() && Serial.read() == 's') {
  lv_demo_widgets();  // 重新显示! (做不了)
}
```

**好吧, 认真说: 最终方案 = 修改 flush callback + 串口**

这是改动最小、最可靠的方案。让我来写。
