# Desk-Mate BLE 通信协议

## 概述

Desk-Mate 通过 BLE GATT 与 PC 端守护程序 (`desk-mate-daemon`) 通信。ESP32-C6 作为 **BLE Peripheral**，PC 作为 **BLE Central**。

## GATT Service

| Service | UUID | 类型 | 说明 |
|---------|------|------|------|
| DeskMate Service | `0000d1ab-0000-1000-8000-00805f9b34fb` | Primary | 核心服务 |

## Characteristics

### 1. TX (DeskMate → PC)

| 属性 | 值 |
|------|-----|
| UUID | `0000d1ab-0001-1000-8000-00805f9b34fb` |
| 属性 | Notify |
| 说明 | DeskMate 向 PC 发送数据 |

**数据格式**: JSON over BLE (每包 ≤ MTU-3 bytes)

```jsonc
// ① Token 计划数据请求
{ "t": "token_req" }

// ② 语音音频数据
{ "t": "audio", "d": "<base64_pcm_chunk>" }

// ③ ACP 响应
{ "t": "acp_resp", "id": "req_001", "a": "confirm" }

// ④ 心跳/状态
{ "t": "ping", "b": 83 }
```

### 2. RX (PC → DeskMate)

| 属性 | 值 |
|------|-----|
| UUID | `0000d1ab-0002-1000-8000-00805f9b34fb` |
| 属性 | Write |
| 说明 | PC 向 DeskMate 发送命令 |

**数据格式**: JSON over BLE

```jsonc
// ① Token 计划更新
{ "t": "token_upd", "d": [
    {"n":"DeepSeek V4","u":"tokens","u_val":1234567,"t_val":2058023,"pct":60},
    {"n":"GPT-4o","u":"$","u_val":23.50,"t_val":50.00,"pct":45}
]}

// ② ACP 请求
{ "t": "acp_req", "id":"req_001", "title":"Send email?", "detail":"To: zhangsan@..." }

// ③ 语音转文字结果
{ "t": "stt_res", "text":"帮我写一个Python爬虫", "conf":0.95 }

// ④ 桌宠打趣话推送
{ "t": "pet_say", "text":"Code works? Must be a fluke." }

// ⑤ 番茄钟同步
{ "t": "pomo_sync", "state":"working", "remain":1500 }

// ⑥ 系统命令
{ "t": "cmd", "cmd":"reboot" }
```

## 音频协议

- 采样率: 16 kHz
- 格式: 16-bit PCM S16LE
- 分片: 每 50ms 音频 = 1600 bytes → Base64 ≈ 2136 bytes → 分 2-3 个 BLE 包发送
- DeskMate 按下麦克风 → 开始发送 `{ "t":"audio", "d":"..." }` → 松开 → 发送 `{ "t":"audio", "d":"end" }` 结束标记

## 连接流程

```
PC                    DeskMate
 |                       |
 |--- Advertisement ----→|
 |←--- Connect ----------|
 |                       |
 |--- Discover Services →|
 |←--- Service Found ----|
 |                       |
 |--- Enable Notify (TX) |
 |←--- Notify Enabled ---|
 |                       |
 |--- { "t":"ping" } ---→|
 |←-- { "t":"ping" } ----|
 |                       |  ← 连接建立完成
```

## 错误处理

- 超时: PC 30 秒未收到 ping → 主动断开 → DeskMate 进入可连接广播
- 重连: DeskMate 断开后自动进入广播模式，PC 端自动扫描重连
- 队列满: 音频数据优先，其他命令降级丢弃
