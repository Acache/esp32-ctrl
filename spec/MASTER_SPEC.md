# MASTER_SPEC — Desk-Mate 语音助手系统规格

> **重建说明（2026-10-25）**：本文件原稿（38KB）写于 `$WORKSPACE_DIR/` 临时目录，未落盘到项目，**已丢失**。
> 本版由**现有代码反推重建**（`mac/src/voice_assistant/` + `firmware/desk-mate-app/` + `README.md`），
> 所有常量、枚举、schema 均从源码逐条核对，**代码是唯一事实来源**。
> 标注 `⚠️ 待确认` 的地方是文档与代码不一致、或缺少原始决策记录的地方。

---

## 0. 项目一句话

> 把一块 ESP32 触摸屏变成 Mac 的**物理入口**：桌面计时器 + 蓝牙媒体键 + 按住说话执行真实应用操作（首个落地场景：B站）。
> 设备只负责**输入、采集、显示**；主机负责**理解、决策、执行**。两端各自独立校验同一份类型化契约。

---

## 1. 系统边界与职责分离

| 侧 | 拥有 | 绝不做什么 |
|---|---|---|
| **设备**（ESP32-C6） | 输入采集、音频环形缓冲、显示状态、本地计时器 | **不决定一条指令的含义** |
| **主机**（macOS） | 语义解析、策略裁决、执行 | **从不渲染 UI** |

- 命令是**类型化契约**（typed contract），两端**各自独立校验**
- 设备侧进程与主机侧服务**故障隔离**：主机挂了，屏 1/2（计时器/媒体）仍可用

---

## 2. 硬件

### 2.1 板子

| 项 | 值 |
|---|---|
| 型号 | **微雪 Waveshare ESP32-C6-Touch-AMOLED-2.16** |
| 显示 | 480×480 AMOLED，CO5300 驱动，GPIO15 = LCD CS |
| 触摸 | CST9220（GPIO5 = INT，GPIO11 = RST）|
| 麦克风 | **ES7210** 双麦，I2S（GPIO19 MCLK / 20 SCLK / 21 DIN / 22 LRCK / 23 DOUT）|
| 电源 | AXP2101 PMIC（GPIO7/8 = I2C SCL/SDA，0x34）|
| IMU | QMI8658（GPIO16 = INT1）|
| RTC | PCF85063（I2C）|
| 存储 | SPI SD 卡（GPIO0 CLK / 1 MOSI / 2 MISO / 3 DATA3 / 4 DATA2 / 6 CS）|

> ⚠️ **待确认（文档冲突）**：
> - `firmware/desk-mate-app/TODO.md` 与 `button_plan.md` / `pinout.md` 写的是 **2.16 寸**
> - 但项目早期记忆里是 **1.43 寸 / 466×466**（另一块板）
> - `README.md` 写 "480×480 AMOLED" —— **与 2.16 寸一致**
> → **以代码与 README 为准 = 2.16 寸 480×480**；若实际持有 1.43 寸板子，则显示层需重做。

### 2.2 三个物理按键

| 按键 | GPIO | 语义 |
|---|---|---|
| **BOOT** | GPIO9（上拉输入，下降沿） | `esp_restart()` 重启 |
| **PWR** | 非 GPIO → AXP2101 PEK | 长按关机 / 单击开机 |
| **KEY** | GPIO10（上拉输入，下降沿） | 预留录音 |

> ⚠️ **待确认**：原始设计里提到"屏 3 按住麦克风图标录音、800ms 内再按取消、POWER 长按 5 秒全局关机"，
> 与现有 `KEY` GPIO10 的实现不完全对应 —— **KEY 按键当前是"预留"，实际录音走触摸图标**。

### 2.3 屏幕分区（LVGL pager）

| 屏 | 内容 | 交互 |
|---|---|---|
| 1 | **Focus 番茄钟** | 触摸开始/暂停/重置（**倒计时** 25 分钟工作 + 5 分钟休息）|
| 2 | **Media 媒体** | BLE HID → macOS 媒体键（播放/暂停/上下曲/音量）|
| 3 | **Voice 语音** | 触摸按住麦克风图标 → 录音；松开发送 |

---

## 3. 通信协议

### 3.1 传输

- **WSS（TLS WebSocket）**，默认端口 **8765**
- 双向 JSON 消息，**客户端（设备）→ 服务端（主机）** 与 **服务端→客户端** 各自一组类型
- 证书校验强制；设备 token 通过 **USB 预配**（`provisioning.py`）

### 3.2 设备 → 主机

| type | 类 | 用途 |
|---|---|---|
| `hello` | `Hello` | 握手，携带设备身份 |
| `begin` | `Begin` | 开始一次命令（携带 command_id）|
| `replay_prepare` | `ReplayPrepare` | 重放准备（复现历史命令）|
| `seal` | `Seal` | 音频封口（标记流结束）|
| `commit` | `Commit` | 提交 → 触发执行 |
| `cancel` | `Cancel` | 取消 |
| `status` | `StatusQuery` | 查询状态 |
| `ping` | `Ping` | 心跳 |
| `confirm` | `Confirm` | 回应确认请求 |
| `feedback` | `Feedback` | 用户 GOOD/BAD 反馈 |

### 3.3 主机 → 设备

| type | 类 | 用途 |
|---|---|---|
| `hello_ack` | `HelloAck` | 握手应答 |
| `begin_ack` / `append_ack` / `seal_ack` | `*Ack` | 各阶段确认 |
| `replay_ready` | `ReplayReady` | 重放就绪 |
| `commit_ack` | `CommitAck` | 提交应答 |
| `cancel_ack` | `CancelAck` | 取消应答（`canceled` / `too_late` / `already_terminal`）|
| `step_result` | `StepResult` | 单步结果 |
| `result` | `Result` | 整体结果 |
| `feedback_ack` | `FeedbackAck` | 反馈应答 |
| `status_snapshot` | `StatusSnapshot` | 状态快照 |
| `progress` | `Progress` | 进度推送 |
| `error` | `ErrorMessage` | 错误 |
| `pong` | `Pong` | 心跳应答 |
| `confirm_preview` / `confirm_required` | `ConfirmPreview` / `ConfirmRequired` | **危险操作二次确认** |

### 3.4 命令状态机

```
STAGING → SEALED → COMMITTED → TRANSCRIBING → PLANNING
   → [AWAITING_CONFIRMATION] → EXECUTING → {SUCCEEDED | PARTIAL | FAILED}
                                    → CANCELED | ABANDONED | INTERRUPTED | OUTCOME_UNKNOWN
```

终态集合：`SUCCEEDED / PARTIAL / FAILED / CANCELED / ABANDONED / INTERRUPTED / OUTCOME_UNKNOWN`

### 3.5 单步状态

`PENDING → PREPARED → STARTED → {SUCCEEDED | NOOP_VERIFIED | FAILED | SKIPPED | OUTCOME_UNKNOWN}`

> `NOOP_VERIFIED` 是重要的设计：**目标已经是那个状态时，也要有可验证的证据**，而不是静默跳过。

---

## 4. 语音管线

```
按键/触摸按住 → I2S 采集 (16 kHz mono s16) → 有界 PCM 环形缓冲
   → 二进制 WS 帧 (WSS) → SenseVoice STT → 规则 planner → [回退] 本地 LLM
   → ValidatedPlan → macOS Accessibility (ax_helper) → Bilibili
   → 结果回传设备；GOOD/BAD 覆盖层喂给个性化闭环
```

### 4.1 STT

| 项 | 值 |
|---|---|
| 模型 | **`iic/SenseVoiceSmall`**（本地）|
| 进程 | `python -m voice_assistant.stt_worker --model <model>` |
| 音频格式 | 16 kHz / 单声道 / 16 bit（**严格校验**，不合规 → `AUDIO_INVALID`）|
| 失败策略 | **fail closed** —— 模型/运行时缺失 → `MODEL_UNAVAILABLE`，**绝不回退云服务** |

### 4.2 决策层（★ 核心）

**关键事实（与早期记忆不同，以代码为准）**：

> `README.md` 明确：**规则 planner 是主路径，本地 LLM 是 fallback**。
> `intent.py` 里有 `rule_based_proposal()`、`apply_policy_guard()`、`has_supported_intent()`。

| 项 | 值 |
|---|---|
| 规则层 | `intent.py` → `rule_based_proposal()` + `apply_policy_guard()` |
| LLM 层 | `decision.py` → Ollama，`http://127.0.0.1:11434` |
| 模型 | **`qwen2.5:1.5b`**（`config.ollama_model`）|
| Prompt | `mac/prompts/intent-v1.txt`（33 行，含 schema 示例）|
| 输出上限 | `MAX_RESPONSE_BYTES = 32 * 1024` |
| 信任模型 | **模型输出是不可信数据**（`decision.py` 文档字符串原话）|

### 4.3 决策提案 schema

```json
{
  "schema_version": 1,
  "status": "ok | clarify_needed | out_of_scope",
  "reason_code": "none | ambiguous | missing_target | unsupported | too_many_steps | policy_denied",
  "clarification": "string | null",
  "model_confidence": 0.0-1.0 | null,
  "actions": [
    {"id": 0, "action": "bilibili.play", "params": {...}, "after": []}
  ]
}
```

`status` 三态设计很关键：
- `ok` —— 直接执行
- `clarify_needed` —— 信息不足，**反问用户**（`clarification` 字段承载问句）
- `out_of_scope` —— 明确超出能力范围，**不猜**

`model_confidence` 可空 —— **不强迫模型每次都给置信度**，但给了就必须合法（0-1）。

### 4.4 动作清单（18 个，⚠️ 早期记忆说 19 个，实际 18）

| 动作 | 参数 | 说明 |
|---|---|---|
| `bilibili.open` | — | 打开 B 站 |
| `bilibili.search` | `query`（≤128 字符 / ≤512 字节）| 搜索 |
| `bilibili.play` | **三选一**：`bv` / (`index`+`search_result_id`) / `query` | 播放 |
| `bilibili.pause` | — | 暂停 |
| `bilibili.resume` | — | 继续播放 |
| `bilibili.next` | — | 下一集 |
| `bilibili.prev` | — | 上一集 |
| `bilibili.favorite` | `video_id?` | 收藏 |
| `bilibili.like` | `video_id?` | 点赞 |
| `bilibili.history` | — | 历史 |
| `bilibili.danmaku_on` | — | 开弹幕 |
| `bilibili.danmaku_off` | — | 关弹幕 |
| `bilibili.send_danmaku` | `text`（≤30 字符 / ≤240 字节）, `video_id?` | 发弹幕 |
| `system.open_app` | `app_id`（小写别名）| 打开应用 |
| `system.vol_up` | `delta`（1-10，默认 5）| 音量+ |
| `system.vol_down` | `delta`（1-10，默认 5）| 音量- |
| `system.mute` | — | 静音 |
| `system.screenshot` | — | 截图 |

> ⚠️ `bilibili.next` / `bilibili.prev` / `bilibili.history` / `bilibili.danmaku_on/off` 在 `ACTION_PARAM_TYPES` 里注册了，
> 但 `adapters/bilibili.py` 的 `probe()` 只探测 `open/search/play/pause/resume` —— **能力探测覆盖不全**，
> 意味着这几个动作在 `probe()` 返回的 `capabilities.available` 里可能缺失，**这是一个潜在缺口**。

### 4.5 跨步骤引用（★ 关键机制）

参数值可以写成 **`$prev.output.<field>`** 形式，指向**前一步的输出**：

```
search_result_id  ← bilibili.search 输出
video_id          ← bilibili.play 输出
app_id            ← system.open_app 输出
player_generation / view_generation ← 播放器/视图快照代数
```

`planner.py` 的 `OUTPUT_FIELD_SOURCE` 明确规定**谁可以产出哪个字段**：

| 字段 | 唯一来源动作 |
|---|---|
| `search_result_id` | `bilibili.search` |
| `video_id` | `bilibili.play` |

`parse_reference()` 解析 `$prev.output.X`；`source == -1` 表示"当前步之前的最后一步"。

> **设计价值**：这让"搜索 → 播第 3 个"这类多步意图可以用**声明式**方式表达，
> 而不需要模型自己记住搜索结果 —— **模型不用处理它看不到的状态**。

### 4.6 依赖与短路

`ActionStep.after: list[int]` 声明**依赖的前置步骤 id**。

- `planner.py` 校验依赖图合法性（不能引用未来的步骤、不能自引用、不能越界）
- **依赖动作失败 → 后续依赖它的动作短路**（不执行），整条命令可能落到 `PARTIAL`
- `ValidatedPlan` 里 `after` 被**排序**（`sorted(a.after)`）保证确定性

### 4.7 计划校验（planner = 唯一可信入口）

`planner.py` 文档字符串原话：

> "The LLM proposal is untrusted data. This module is the only place that turns it into a ValidatedPlan."

校验内容：
- 动作名在白名单内（`ACTION_PARAM_TYPES`）
- 参数通过 Pydantic 校验（`_Strict` 模型 → 禁止多余字段）
- 依赖图合法
- 引用字段的产出者匹配（`OUTPUT_FIELD_SOURCE`）
- **目标新鲜度**：`player_snapshot` / `search_snapshot` 过期 → `TARGET_STALE`
- 参考目标解析（`parse_reference`）

### 4.8 确认门（危险操作二次确认）

- `ConfirmPreview` / `ConfirmRequired` 消息
- `Confirm` 客户端响应
- 超时 → `CONFIRM_EXPIRED`
- `planner._preview_for()` / `pipeline._account_for()` 生成预览

---

## 5. 存储（SQLite，14 表）

### 5.1 表清单（`migrations/001_initial.sql`）

| 表 | 用途 |
|---|---|
| `schema_migrations` | 迁移版本 |
| `devices` | 设备注册 |
| `commands` | 命令主表（状态机落库）|
| `action_steps` | 单步明细 |
| `audio_samples` | 音频留存 |
| `feedback` | 用户反馈 |
| `preferences` | 偏好 |
| `model_versions` | 模型版本指纹 |
| `training_runs` | 训练运行 |
| `command_tombstones` | 墓碑（防重放）|
| `snapshots` | 播放器/视图快照（带 generation）|
| `confirmations` | 确认记录 |
| `account_rate_limits` | 账号级限流 |
| `device_sessions` | 设备会话 |

索引：`idx_commands_device_created` / `idx_commands_state` / `idx_commands_replay_of` / `idx_snapshots_generation`

### 5.2 `002_feedback.sql` 增量

给 `feedback` 加：`rating` / `reason` / `dismissed_by` / `touch_count` / `countdown_at_dismiss` / `context_json` / `source`（默认 `device`）/ `received_at`
新增表：`feedback_analysis_runs`

---

## 6. 学习闭环（完全本地）

```
设备 GOOD/BAD 覆盖层 → feedback 表
   → feedback_analysis.py（分析）
   → auto_training.py（自动触发训练）
   → training.py（训练）
   → personalization.py（个性化参数）
   → preferences 表
   → 下次决策生效（model_versions 记录指纹）
```

配套：`scripts/train-personalization.sh`、`install-training-reminder-agent.sh`（定时提醒 Agent 训练）
`auto_training.py` 里 `training_reminder_loop()` 在 `api.py` 的 lifespan 中运行。

---

## 7. 配置

| 项 | 默认 |
|---|---|
| 端口 | 8765 |
| 成功留存 | 1825 天 |
| 失败留存 | 1825 天 |
| 音频配额 | 16 GB |
| STT 后端 | `auto` |
| Ollama 模型 | `qwen2.5:1.5b` |
| Ollama URL | `http://127.0.0.1:11434` |

数据目录权限：`secure_makedirs(mode=0o700)`，文件 `0o600`（`write_private`）。

---

## 8. HTTP / WS 接口

| 方法 | 路径 | 用途 |
|---|---|---|
| GET | `/healthz` | 健康检查（含 `active_command`）|
| GET | `/v1/status/{command_id}` | 命令状态 |
| WS | `/v1/remote` | 主协议通道 |

**WS 连接管理**：`ConnectionManager`（`api.py`）
- `attach(device_id, ws)` —— 同设备旧连接被**顶掉**（新连接接管）
- `detach` / `send` / `close`
- `notify_for(device_id)` —— 异步推送

---

## 9. 安全属性

| 属性 | 实现 |
|---|---|
| **动作白名单** | `ACTION_PARAM_TYPES` —— 无任意脚本执行 |
| **TLS 证书校验** | 强制 |
| **设备 token 预配** | USB 通道（`provisioning.py`）|
| **有界超时** | 所有外部调用都有 timeout |
| **背压控制** | 音频配额 + 环形缓冲上限 |
| **故障隔离** | 主机服务挂掉不影响屏 1/2 |
| **重放防护** | `command_tombstones` |

---

## 10. 环境要求（★ 有坑）

| 工具 | 版本 | 用途 |
|---|---|---|
| macOS | 13+ | 主机服务、辅助功能 |
| **Python** | **3.11（不是 3.12+）** | `torch==2.14` / `funasr==1.4` 无 3.12+ wheel |
| arduino-cli | 1.0+ | 固件 |
| ESP32 core | 3.3.x (`esp32:esp32`) | 固件 |
| Xcode CLT | 较新 | `swiftc` 编译 `ax_helper` |
| Bilibili 桌面版 | 最新 | 语音控制目标 |

---

## 11. 代码地图

```
mac/src/voice_assistant/
├── contracts.py       (1112) 类型契约：Envelope、状态机、错误码、决策 schema
├── storage.py          (742) SQLite 持久层 + 迁移
├── config.py           (356) 配置、身份生成、权限
├── cli.py              (355) 命令行
├── pipeline.py         (384) 编排：COMMITTED → STT → 计划 → 执行
├── sessions.py         (409) 会话与状态机驱动
├── stt.py              (332) SenseVoice 接入
├── executor.py         (306) 执行器
├── intent.py           (272) 规则 planner + 策略护栏
├── planner.py          (243) 计划校验与目标绑定 ★ 唯一可信入口
├── decision.py         (211) Ollama 决策适配器（输出不可信）
├── provisioning.py     (190) 设备 token 预配
├── feedback_analysis.py(162) 反馈分析
├── personalization.py  (148) 个性化
├── training.py         (143) 训练
├── auto_training.py    (135) 自动训练触发
├── api.py                     FastAPI + WS
├── stt_worker.py        (81)  STT 子进程
├── discovery.py         (62) 设备发现
├── ax_daemon.py        (104) 辅助功能守护
├── adapters/
│   ├── base.py
│   ├── bilibili.py      B站适配（含 ax_helper 调用）
│   ├── macos.py         系统级动作
│   └── fake.py          测试替身
├── migrations/
│   ├── 001_initial.sql
│   └── 002_feedback.sql
└── prompts/intent-v1.txt  决策 prompt（33 行）

tests/
├── unit/{test_contracts,test_feedback,test_intent,test_planner_executor,test_storage}.py
└── integration/{test_protocol,test_tls}.py

firmware/desk-mate-app/
├── TODO.md / button_plan.md / pinout.md
└── focus_ui.h / focus_model.h
```

---

## 12. 已知缺口与待办

| # | 项 | 状态 |
|---|---|---|
| 1 | **硬件型号冲突**（2.16 vs 1.43）| ⚠️ 需确认实机 |
| 2 | **KEY 按键**（GPIO10）语义未定 | ⚠️ 录音走触摸，KEY 预留 |
| 3 | `probe()` 未覆盖全部动作 | ⚠️ 潜在能力缺口 |
| 4 | 原始 spec 的"19 个动作" | ⚠️ 实际 18 个 |
| 5 | `MASTER_SPEC.md` 原稿 | ❌ 已丢失（本文件为重建版）|
| 6 | `bilibili.send_danmaku` 置信度门槛 | ⚠️ 早期记忆提到"三层防护"，代码中**未见置信度门槛实现** —— 需确认是设计意图还是待实现 |
| 7 | 弹幕 30 字净化 + 30 秒限流 | ⚠️ 30 字有校验；30 秒限流**未在代码中见到** |

---

## 13. 供 review 的三个开放问题

1. **规则优先 vs LLM 优先** —— README 说规则主、LLM 备，但 `decision.py` 的实现量更大。是刻意的"先规则兜底再上 LLM"，还是应该反过来？
2. **置信度门槛** —— `model_confidence` 字段存在但未见消费方。是否应该设阈值（低于 X 就走 `clarify_needed`）？这直接关系到"该不该硬猜"。
3. **18 个动作的取舍** —— `bilibili.next/prev/history/danmaku_on/off` 注册了但能力探测没覆盖。要么补探测，要么从白名单移除 —— 保留"声明了但用不了"的动作是负债。

---

*重建于 2026-10-25 · 依据：`bili-esp32-ctrl` commit `c7c7760` 工作区状态*
