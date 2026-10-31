# Desk-Mate × bili-esp32-ctrl 合并方案

> 分析日期：2026-10-31
> 对象：`desk-mate@3b07770`（含未提交工作区）× `bili-esp32-ctrl@c7c7760`（含未提交工作区）
> 结论：**desk-mate 做唯一 trunk，用 git 双历史合并把 bili 全部收进来**；合并本身几乎没有代码冲突，真正要做的是"清理 + 文档归一 + 为下一轮迭代腾容量"。

---

## 0. 结论（先看六条）

1. **这不是两个并行项目。** bili 是 desk-mate 2026-09-24 工作区的衍生副本，随后单向演进了 3 周。两者共用同一块板子、同一份板级驱动、同一套 Focus/Media UI。
2. **代码冲突极小。** 两边同名文件 31 个：23 个字节完全一致，只有 **8 个不同**；其中 **7 个 bili 是严格超集**，只剩 `README.md` 需要人工重写。
3. **desk-mate 的 HEAD 现在编译不过**（`lv_font_montserrat_24` 未声明），因为它依赖一处从未提交的 `lv_conf.h` 修改；bili 恰好把这处修改带走了。也就是说：**bili 是唯一可编译的已提交状态，desk-mate 只有工作区能编译**。
4. **合并方式**：desk-mate 当 trunk，用 `git merge --allow-unrelated-histories` 保留两边历史（desk-mate 8 个提交 + bili 1 个快照），再分阶段清理目录。不要用"复制粘贴"或"以 bili 为基"的方式，会丢历史。
5. **合并解决"版本/功能分叉"，但不解决"放不下"**：当前固件 2,007,550 / 2,097,152 B = **95.7%**，2MB app 分区只剩约 90KB。16MB flash 目前只划了 4MB 分区。**动下一步功能前必须先改分区表**（方案已实测，见 §10.1）。
6. **v1 的"7 项功能全 ✅"是文档幻觉。** git 历史里从来没有 BLE GATT JSON / Token 额度 / ACP 的实现；v1 只有 866 行未提交的 UI 草稿。合并不会丢掉任何"还能跑的旧功能"，只会丢掉草稿。

---

## 1. 时间线与分叉证据

| 时间 | desk-mate | bili-esp32-ctrl |
|---|---|---|
| 2026-05-29 ~ 06-02 | 板级 bring-up、v1 UI 草稿、硬件踩坑日志 | — |
| 2026-06-01 | 提交 `a671d2f` v1.0（只有 34 行 .ino + 326 行 UI 头） | — |
| 2026-09-24 09:53–12:29 | 提交 6 个：两屏 Focus+Mac UI → 手势化计时器 → 大字号 → 滚轮提示 → 阴影 | — |
| 2026-09-24 之后 | **停止提交**，工作区留下未提交的 `lv_conf.h` / `ui_pages.h` 修改 | 从该工作区复制出仓库 |
| 2026-10-17 ~ 10-18 | — | 增加语音页、bili_voice 固件、mac 主机服务、测试、内存调优 |
| 2026-10-18 23:56 | — | 单提交 `c7c7760`（压缩快照） |
| 2026-10-25 | — | 重建 `MASTER_SPEC.md`（**未跟踪**） |
| 2026-10-27 | — | 最后的脚本调整（`notify-feishu.sh` **未跟踪**，且是 DSH 工具，不属于项目） |

**分叉证据（都是硬证据）**

- 23 个同名文件字节一致：`lcd_touch.*`、`bsp_lvgl_port.h`、`media_ble.h`、`focus_model.h`、`user_config.h`、`src/port_bsp/*`、`src/externLib/esp_lcd_sh8601.*`、`pinout.md`、`button_plan.md`、`TODO.md`、`firmware/README.md`、`tests/focus_model_test.c`、`tools/lv_sim/{lv_conf.h,sim_hal.*}`。
- bili 的编译产物名仍是 `desk-mate-app.ino.bin`；串口 banner 是 `Desk-Mate 2 / Focus + Mac + Bilibili`；launchd label 是 `com.desk-mate.voice-assistant`；目录仍叫 `firmware/desk-mate-app`。
- bili **没有**带走 desk-mate 的 `docs/`、`log/`、`spec/`、`.learnings/`、`firmware/touch_probe/`、v1 素材 —— 所以这些只能从 desk-mate 这一侧继承。

**⚠️ 事实纠正**：desk-mate 的 `README.md` 与 `TODO.md` 描述的是 v1 七项功能（桌宠/额度/ACP/蓝牙页/录音），但 `a671d2f` 里只有 34 行 `.ino` + 326 行纯 UI 头，**没有任何 BLE JSON / ACP 实现**；924 行那版只存在于某次工作区，已被 9-24 重写覆盖。`spec/ble-protocol.md` 是设计稿，不是已实现协议。合并时要把这些文档降级为"设计/路线图"。

---

## 2. 8 个差异文件逐个定性

| 文件 | desk-mate | bili | 处置 | 理由 |
|---|---|---|---|---|
| `firmware/desk-mate-app/desk-mate-app.ino` | 89 行 | 152 行 | **取 bili** | 多出语音页、按键跨页归属抑制、PMIC 短路进语音页、状态日志 |
| `firmware/desk-mate-app/focus_ui.h` | 332 行 / 19.6KB | 756 行 / 41.7KB | **取 bili** | 相对 desk-mate 现版 **+433 / −13 行**，被替换的 13 行全是"两页数组→三页数组"的机械推广；desk-mate 的滚轮/阴影/手势逻辑全部在内 |
| `firmware/desk-mate-app/lv_conf.h` | 48KB LVGL 池 | 32KB LVGL 池 | **取 bili** | 为 WiFi+TLS+BLE 让内存；字体开关两边一致 |
| `firmware/desk-mate-app/bsp_lvgl_port.cpp` | 50 行×2 绘制缓冲 | 10 行×2 | **取 bili** | 省约 57KB 内部 RAM；双缓冲与 DMA 语义不变 |
| `tools/lv_sim/sim_main.c` | 229 行 | 345 行 | **取 bili** | 三页断言 + Bili 页刷新 + 反馈 overlay 状态机测试 |
| `tools/lv_sim/CMakeLists.txt` | — | 多 `-UNDEBUG` | **取 bili** | Release 下让 `assert` 继续生效（这就是它的测试） |
| `.gitignore` | 14 行 | 43 行 | **手工并集** | 两边规则语义冲突，见 §8.3 |
| `README.md` | 中文、v1 已过时 | 英文、贴合当前架构 | **重写** | 根 README 用中文总览；bili 版转存 `docs/voice-assistant.md` |

> 结论：**6 个取 theirs、1 个手工并集、1 个重写**。没有任何需要三方手工合并的代码逻辑。

---

## 3. bili 新增了什么（要迁入的能力）

| 模块 | 规模 | 说明 |
|---|---|---|
| `firmware/desk-mate-app/src/bili_voice/` | 2,630 行 C/C++（12 文件） | I2S 采集→有界 PCM 环→手写 WSS/TLS 客户端→配网(USB/SoftAP)→语音 FSM→UI 状态桥 |
| `mac/src/voice_assistant/` | 9,112 行 Python（26 文件） | SenseVoice STT、规则 planner、本地 LLM 回退、执行器、SQLite(14 表)、反馈/个性化、FastAPI/WS、CLI |
| `mac/scripts/ax_helper.swift` | 820 行 | macOS Accessibility 执行代理 |
| `mac/tests/` | 7 文件 / 69 用例 | 契约、反馈、意图、计划执行、存储、协议、TLS |
| `firmware/test/` | 2 文件 | 语音 FSM、音频环形缓冲的原生 C 测试（**当前没有构建入口**，见 §9.6） |
| `scripts/` | 4 文件 | 配网、打包、训练、基线抓取 |
| `bootstrap.sh` / `run.sh` / `install-*.sh` | — | venv、WSS 服务、launchd |
| `MASTER_SPEC.md` | 17.7KB | **未跟踪**，必须单独复制 |
| `LICENSE` | MIT | 迁入 |
| `mac/prompts/intent-v1.txt`、`requirements.lock` | — | 迁入 |

## 4. desk-mate 独有、需要决定去留的资产

| 资产 | 规模 | 建议 |
|---|---|---|
| `log/*.md` + `.learnings/*` | 923 行 | **保留**（白屏/I2C/字体崩溃等真实踩坑，bili 已丢失） |
| `docs/superpowers/{plans,specs}` | 6 文件 | **保留**（9-24 迭代的设计与计划） |
| `docs/DeskMate_Design_v12.pdf`、板卡 md、`arduino_knowledge_base.md` | — | **保留**，归到 `docs/hardware/` |
| `spec/deskmate-app-design.md`、`spec/ble-protocol.md` | 854 行 | **归档到 `spec/archive/`**，标注"设计稿，未实现" |
| `firmware/touch_probe/` | 6 文件 | **保留**（触摸/PMIC 诊断草图，bili 丢了） |
| `firmware/desk-mate-app/ui_pages.h`（866 行，未提交版） | — | **删除**，打 tag `v1-legacy-ui-final` 保留在历史里 |
| `einstein_*`、`font_cn_*`、`font_tnr_20.c`、`jokes.h` | ~20MB | **删除**（v1 桌宠素材，当前 UI 不用；历史可查） |
| `*.bak` / `*.oldbak` / `.DS_Store` | — | **删除** |
| `tools/lv_offline_sim/`（含 381 个被跟踪的 build 产物） | 386 个被跟踪文件 | **删除**，仅把 `convert.py` 移到 `tools/convert_screenshot.py` |
| `simulator/`（lv_port_pc_vscode 克隆，1.0GB） | 已忽略 | **删除**（`tools/lv_sim` 已取代） |
| `tools/{auto_checker,screenshot_viewer,screenshot_wifi}.py`、`design_v3.c` | — | **删除**（依赖 v1 板端 HTTP 截图接口，现已不存在） |
| `tools/lv_offline_sim/lvgl` 符号链接 | — | 随目录删除 |

---

## 5. 实测数据（2026-10-31 本机跑过）

| 项目 | 结果 |
|---|---|
| bili 模拟器 `cmake --build` + `ctest` | **2/2 通过**（timer、swipe_and_ui 含三页/反馈断言） |
| bili 主机单测 `pytest mac/tests/unit` | **69 passed / 15.6s** |
| bili 原生 C 测试（手工链接 `voice_fsm.c` / `audio_convert.c`+`pcm_ring.c`） | **两套全 PASS** |
| bili 固件编译（`CDCOnBoot=cdc,FlashSize=16M,PartitionScheme=no_ota`） | **2,007,550 B = 95.7%**；静态 RAM 146,344 B（44%） |
| desk-mate HEAD 固件编译（同样 FQBN） | **失败**：`lv_font_montserrat_24` 未声明 |
| desk-mate 工作区（补上未提交 lv_conf）编译 | 1,136,500 B = 54%；静态 RAM 76,004 B（23%） |
| **语音功能代价** | **+871KB flash、+70KB 静态 RAM** |
| 当前分区表 | app0 2MB + spiffs 1.875MB + coredump 64KB = **仅用 4MB / 16MB** |
| 自定义 8MB app 分区实测 | 编译通过：2,007,358 B（按 16MB 上限报 11%），分区表生成正确（app0 8MB、spiffs 7.9MB） |

内存/容量的现实：下一步功能只能在这 ~90KB flash 和 ~181KB 动态 RAM 里做文章 —— 除非先改分区表。

---

## 6. 合并方案对比

| 方案 | 做法 | 历史 | 风险 | 评价 |
|---|---|---|---|---|
| **A（推荐）** | desk-mate 为 trunk，`git merge --allow-unrelated-histories bili/main`，保留双亲 | ✅ 两边历史 + 一个 merge commit | 极低（8 个冲突且方向明确） | **采用** |
| B | 直接把 bili 文件拷进 desk-mate | ❌ 丢失分叉记录 | 低 | 只适合一次性脚本，不推荐 |
| C | 以 bili 为基，把 desk-mate 文档拷过去 | ❌ desk-mate 8 个提交被降级为"外来历史"，`docs/log/spec` 无归属 | 中（还要处理两条 root） | 与"在 desk-mate 里维护"的意愿相反 |

**为什么 A 对"继承性"最好**：merge 之后，`git log` 同时能看到 5-6 月的板级踩坑、9-24 的 UI 迭代、10 月的语音能力；`git log --follow firmware/desk-mate-app/focus_ui.h` 能把两段历史接起来；tag 把两个快照钉死，随时可对照。

---

## 7. 目标结构

### Phase 1（合并当天，只做必要动作，保持路径不变）

```
desk-mate/
├── README.md                  # 新写：中文总览
├── LICENSE                    # ← bili (MIT)
├── VERSION / CHANGELOG.md     # 新增，见 §10.4
├── docs/
│   ├── voice-assistant.md     # ← bili README 原样（英文架构文）
│   ├── lineage.md             # 新增：血统、分叉点、合并记录、v1 事实纠正
│   ├── hardware/              # 板卡 md / v12 pdf / 知识库（pinout、button_plan 留在 firmware/）
│   ├── memory-budget.md       # 新增，见 §10.5
│   └── superpowers/…          # 原样保留
├── spec/
│   ├── MASTER_SPEC.md         # ← bili 根目录（未跟踪文件，手工复制）
│   └── archive/               # v1 设计稿：ble-protocol / deskmate-app-design / screenshot_design
├── firmware/
│   ├── desk-mate-app/         # bili 版为基准 + 后续加 partitions.csv
│   ├── touch_probe/           # 保留
│   └── test/                  # ← bili 原生测试（下一步加 CMake/Makefile）
├── mac/                       # ← bili 主机服务（Phase 2 可改名 host/）
├── scripts/                   # ← bili + 新增 build/test/setup 脚本
├── tools/
│   ├── lv_sim/                # 唯一模拟器（bili 版）
│   └── convert_screenshot.py  # 从 lv_offline_sim 提取
├── tests/focus_model_test.c   # 保留（lv_sim/CMakeLists 引用此路径）
└── log/                       # 保留硬件踩坑史
```

### Phase 2（稳定后再动，可选）

- `mac/` → `host/`（配合 Windows 路线图）；同步改 `bootstrap.sh`、`run.sh`、`install-*.sh`、`README`、`package-release.sh`（约 15 处引用）。
- `src/bili_voice/` → `src/voice/`（模块本身与 B 站无关，适配器在主机侧）。
- `tests/focus_model_test.c` → `firmware/test/`，同步改 `tools/lv_sim/CMakeLists.txt` 的 `../../tests/...` 路径。
- `firmware/touch_probe` 与主 sketch 的 `port_bsp` 去重（做成 Arduino library 或符号链接）。

---

## 8. 执行清单（可直接照做）

### 8.1 预检

```bash
cd /Users/acache/AI_PROJ/desk-mate
git status --short          # 确认只剩已知修改
git switch -c merge/bili-import
```

### 8.2 固化工作区（关键：否则 merge 前会有脏文件）

```bash
git add -A
git commit -m "chore(pre-merge): freeze working tree (June UI draft + lv_conf fonts)"
git tag v1-legacy-ui-final       # ui_pages.h 的最后一版，随时可回查
git tag pre-bili-merge
```

### 8.3 引入 bili 历史

```bash
git remote add bili /Users/acache/AI_PROJ/bili-esp32-ctrl
git fetch bili main
git tag bili-snapshot-2026-10-18 c7c7760
git merge --allow-unrelated-histories --no-commit bili/main
```

预期只有 8 个冲突（其余同名文件字节一致，git 会自动合并）。

### 8.4 解决冲突

```bash
# 6 个代码/构建文件：全部取 bili
git checkout --theirs -- \
  firmware/desk-mate-app/desk-mate-app.ino \
  firmware/desk-mate-app/focus_ui.h \
  firmware/desk-mate-app/lv_conf.h \
  firmware/desk-mate-app/bsp_lvgl_port.cpp \
  tools/lv_sim/CMakeLists.txt \
  tools/lv_sim/sim_main.c

# README：先取 bili，下一步重写
git checkout --theirs -- README.md

# .gitignore：手工并集（见下）
$EDITOR .gitignore

git add -A
git commit -m "merge: import bili-esp32-ctrl (voice page + host service) into desk-mate"
```

**`.gitignore` 并集建议**（去掉 desk-mate 的全局图片忽略，去掉 bili 里"忽略 .learnings / 忽略 v1 素材"的规则）：

```gitignore
# 本地引用（不入库）
_lv_sim/
_waveshare_examples/
simulator/lv_port_pc_vscode/
firmware/libraries/
.venv/

# 构建产物
build/
release/
cmake-build-*/
*.o
*.d
*.raw
.DS_Store

# Python
**/__pycache__/
*.py[cod]
.pytest_cache/
*.egg-info/

# 运行时 / 私有（绝不入库）
private/
chat_log.md
tmp_half_done.md
*.db
*.db-wal
*.db-shm
*.pem
*.key
*.crt
*.p12
*.wav
*.aiff
*.log

# 工具产物
mac/bin/
.playwright-cli/
.tmp/
.vscode/
.idea/
```

### 8.5 补导未跟踪文件

```bash
cp /Users/acache/AI_PROJ/bili-esp32-ctrl/MASTER_SPEC.md spec/MASTER_SPEC.md
git add spec/MASTER_SPEC.md
git commit -m "docs: import MASTER_SPEC (untracked in bili, reconstructed 2026-10-25)"
```

> `scripts/notify-feishu.sh` **不要导**：它依赖 `DSH_HOME`/`SESSION_MAP`，是 DSH 工具，不是项目代码。

### 8.6 清理 v1 与老工具（单独一个 commit，便于回退）

```bash
# 先抢救有用的：raw→png 转换器
git mv tools/lv_offline_sim/convert.py tools/convert_screenshot.py

# 老的模拟器/克隆/构建产物（含被跟踪的 381 个 build 文件、指向 ~/Documents 的 lvgl 符号链接）
git rm -r -q tools/lv_offline_sim simulator
git rm -q tools/auto_checker.py tools/screenshot_viewer.py tools/screenshot_wifi.py \
           tools/design_v3.c tools/build_lv_sim.sh

# v1 素材与备份文件
git rm -q firmware/desk-mate-app/ui_pages.h \
           firmware/desk-mate-app/ui_pages.h.bak \
           firmware/desk-mate-app/desk-mate-app.ino.bak \
           firmware/desk-mate-app/lv_conf.oldbak \
           firmware/desk-mate-app/einstein_* \
           firmware/desk-mate-app/font_cn_*.c \
           firmware/desk-mate-app/font_tnr_20.c \
           firmware/desk-mate-app/jokes.h

git commit -m "chore: drop v1 UI remnants, dead tooling and tracked build artifacts"

# 物理清掉未跟踪的残留（1.0GB 的 lv_port_pc_vscode 克隆、构建产物）
rm -rf simulator tools/lv_offline_sim
```

> 381 个 `tools/lv_offline_sim/build/**` 被跟踪，这一步会全部移出（仓库体积与 `git status` 噪音的来源）。
> `font_cn_*.c` 是 `lv_font_conv` 生成物（17MB），删除前把生成参数记进 `docs/hardware/`，以后要中文 UI 可以重生成。

### 8.7 文档归一

```bash
mkdir -p docs/hardware spec/archive
git mv docs/ESP32-C6-Touch-AMOLED-2.16\(带锂电池\).md docs/hardware/
git mv docs/DeskMate_Design_v12.pdf docs/arduino_knowledge_base.md docs/hardware/
git mv spec/ble-protocol.md spec/deskmate-app-design.md spec/screenshot_design.md spec/archive/
cp README.md docs/voice-assistant.md   # 此时 README 已是 bili 版；随后再重写根 README
```

> `pinout.md` / `button_plan.md` 留在 `firmware/desk-mate-app/`（`firmware/README.md` 与 bili README 都以该路径引用它们）；`docs/README.md` 保留并改成 docs 索引。

然后手写：
- `README.md`（中文总览：三页能力、设备/主机分工、A-D 四条上手路径、FQBN 固定写法、测试命令、目录、路线图）
- `docs/lineage.md`（本文件 §1/§2 的浓缩版 + 两个 tag + "v1 只存在于文档"的纠正）
- `firmware/desk-mate-app/TODO.md`（当前它写"7 项全 ✅"，改成真实路线图）
- `spec/archive/README.md`（说明这些是设计稿，未实现，别当协议用）

> 顺手把硬件型号的矛盾统一掉：代码用的是 **SH8601 驱动 + 触摸 I2C 0x5A**；`pinout.md`/`docs/README.md` 写 CO5300，`MASTER_SPEC` 写 CST9220，bili README 写 FT5x06，而 6 月日志明确说"触摸型号未确认，不是 CST816/FT5x06 的寄存器布局"。文档统一写成"SH8601 驱动 / 触摸 I2C 0x5A（疑似 CST9220，待确认）"，别再抄错。

### 8.8 构建/测试入口固定

新增：

```
scripts/setup-lvgl.sh     # clone LVGL 9 到 _lv_sim/lvgl（或接受 LVGL_DIR）
scripts/fw-build.sh       # 固定 FQBN：CDCOnBoot=cdc,FlashSize=16M
scripts/fw-flash.sh       # upload --input-dir，整片烧录
scripts/sim-test.sh       # cmake -S tools/lv_sim -B build/sim && ctest
scripts/host-test.sh      # PYTHONPATH=mac/src .venv/bin/python -m pytest mac/tests -q
scripts/fw-test.sh        # 编译并运行 firmware/test/*.c
scripts/test-all.sh       # 上面四个串起来
firmware/arduino-libraries.txt   # 固定版本
```

`firmware/arduino-libraries.txt` 按本机实测写入：
```
esp32:esp32@3.3.8-cn
lvgl@8.4.0
NimBLE-Arduino@2.5.0
XPowersLib@0.3.3
ArduinoJson@7.4.3
```

### 8.9 验证

```bash
scripts/sim-test.sh          # 期望 2/2
scripts/host-test.sh         # 期望 69 passed
scripts/fw-test.sh           # 期望两套 PASS
scripts/fw-build.sh          # 期望 2,007,xxx B
```

硬件在的时候再做：整片烧录 → 串口 `READY heap=...` → 三页滑动 → PWR/KEY 语义 → 主机 `cli doctor`。

### 8.10 收尾

```bash
git tag merge-bili-2026-10-31
git switch main
git merge --ff-only merge/bili-import
```

远端建议：bili 的 `origin` 是 `github.com/Acache/esp32-ctrl`。两条路选一条：
1. **把 GitHub 仓库改名 `desk-mate`**，把合并后的 main 推上去（快进，不丢 issue/star）；
2. 新建 `desk-mate` 仓库，推合并历史；`esp32-ctrl` 打 `archived`。

本地 `bili-esp32-ctrl/` 目录**先别删**，保留 2-4 周做对照；确认无遗漏后再归档。

---

## 9. 风险与坑（按重要性）

1. **`MASTER_SPEC.md` 是未跟踪文件**，`git merge` 不会带过来 —— 必须手工 `cp`。`scripts/notify-feishu.sh` 同理，但它是 DSH 工具，不要导。
2. **desk-mate HEAD 编译不过**。先提交工作区再 merge；否则 merge 后的"验证"会拿一个本来就坏的基线做对比。
3. **`.gitignore` 语义冲突**：bili 的规则会忽略 `.learnings/` 和 v1 素材；desk-mate 的规则会忽略所有 `*.png/*.jpg/*.bmp`。并集时必须逐条确认，否则以后新增的图片/学习记录会"凭空消失"。
4. **v1 已实现功能是幻觉**：不要为了"继承"去恢复 `ui_pages.h`；它没有 BLE/ACP/额度的实现，只有 UI 排版。保留 tag 即可。
5. **主机 venv 1.4GB 不能直接搬**：venv 里有绝对路径和 shebang。合并后新建 venv 重装（`requirements.lock` 里有 torch/funasr，需网络与时间）；临时过渡可以让 `desk-mate/.venv` 软链到 bili 的 venv。
6. **原生 C 测试没有构建入口**：`firmware/test/*.c` 需要链接 `voice_fsm.c`、`audio_convert.c`、`pcm_ring.c` 才能过；补 Makefile/CMake 并纳入 CI。
7. **两套 LVGL**：固件 8.4、模拟器 9.x。`focus_ui.h` 靠版本宏同时兼容两边；`tools/lv_sim` 还提供 `-DLVGL8_DIR` 的 v8 目标。合并后要在 CI 里同时跑 v8/v9 目标，否则"模拟器过了、硬件炸了"会重演（`.learnings` 里的字体崩溃就是这么来的）。
8. **分区表变更必须整片重刷**：只 upload app 不会更新 0x8000 的分区表。用 `--input-dir build/firmware` 或 `merged.bin` 整片烧，先擦除。
9. **bili 文档承诺的 8 个 docs 文件不存在**：`scripts/package-release.sh` 引用了 `DELIVERY_REPORT.md`、`INSTALL.md`、`OPERATIONS.md` 等，bili 的 `docs/` 是空的。合并后要么补文档，要么把脚本里的 `cp` 改成可选并去掉不存在的名字。
10. **未跟踪的 147MB `build/`、16MB `private/`、120KB `chat_log.md`**：一律不入库；`private/backup/desk-mate-original-16mb.bin` 是整片 flash 备份，建议挪到 `~/AI_PROJ/_backups/` 并在文档里记路径。

---

## 10. 合并后第一件事：给 ESP32 迭代腾空间

### 10.1 分区表（已实测）

现状：`PartitionScheme=no_ota` 只给出 **2MB app**，而固件已经 1.9MB；16MB flash 里 12MB 没被分区表使用。

做法：在 `firmware/desk-mate-app/` 放 `partitions.csv`，FQBN 用 `PartitionScheme=custom`（该选项的 `upload.maximum_size=16MB`，不会误报超限）。

```csv
# Desk-Mate 16MB: 8MB app (no OTA) + 7.9MB storage
# Name,   Type, SubType, Offset,   Size,     Flags
nvs,      data, nvs,     0x9000,   0x5000,
otadata,  data, ota,     0xe000,   0x2000,
app0,     app,  ota_0,   0x10000,  0x800000,
spiffs,   data, spiffs,  0x810000, 0x7E0000,
coredump, data, coredump,0xFF0000, 0x10000,
```

```bash
arduino-cli compile \
  --fqbn 'esp32:esp32:esp32c6:CDCOnBoot=cdc,FlashSize=16M,PartitionScheme=custom' \
  --build-path build/firmware firmware/desk-mate-app
```

本机验证结果：编译通过，占用 **11%**，生成的分区表为 app0=0x800000、spiffs=0x7E0000。

如果要 OTA，把 app0/app1 各设 6MB（`default_16MB.csv` 就是 6.25MB×2），但 OTA 会占掉一半 flash；当前阶段建议 no-OTA 单 app，留 7.9MB 给未来的音频/字库/模型缓存。

### 10.2 代码结构：从"一个静态头"到"页注册表"

`focus_ui.h` 现在是 756 行、全部 `static`、被 `.ino` 和 `sim_main.c` 同时 include 的"单体头"。再加第 4、5 页会迅速失控。建议：

- 拆成 `ui/pages/focus_page.c`、`media_page.c`、`voice_page.c` + `ui/ui_core.c`（pager/dots/导航）。
- 定义 `ui_page_t { build; refresh; on_action; on_key; }` 注册表；`.ino` 只做 `ui_init()`/`ui_tick()`。
- 每页自带输入归属（现在 `.ino` 里 `owner_page/suppressed` 的手写逻辑迁进页的 `on_key`）。
- `sim_main.c` 通过同一个注册表构建，不再复制布局代码。

### 10.3 协议单一事实来源

主机有 `contracts.py`（PROTOCOL_VERSION=1、18 个 action、状态机），固件有 `voice_contracts.h`。两边手写，**必然漂移**。建议加一个生成/校验步骤：

- `scripts/gen_contracts.py`：从 `contracts.py` 导出 `voice_contracts_generated.h`（枚举、action 字符串、错误码），固件只 include 生成物；
- 或最小方案：`host/tests/test_contract_parity.py` 解析 `voice_contracts.h` 宏，逐项对比 `contracts.py`，CI 里跑。

### 10.4 版本与构建固定

- 根 `VERSION`（如 `0.3.0`）+ `CHANGELOG.md`；
- 固件串口 banner 打印 `FW_VERSION`，主机 `cli --version`，两边协议版本一并打印；
- `firmware/arduino-libraries.txt` 固定 §8.8 的版本；`scripts/fw-build.sh` 固化 FQBN（`CDCOnBoot=cdc` 漏了就"刷完没串口"）；
- CI：固件编译 + 模拟器 ctest（v8/v9）+ 主机 pytest + 原生 C 测试。

### 10.5 内存预算文档

把这几个数写进 `docs/memory-budget.md` 并长期维护：

| 项 | 值 |
|---|---|
| flash 总/已用（2MB 分区时代） | 2,097,152 / 2,007,550（95.7%） |
| 语音模块增量 | +871KB flash、+70KB 静态 RAM |
| 静态 RAM | 146,344 B（44%），剩 181,336 B |
| LVGL 池 | 32KB（bili 从 48KB 下调） |
| 绘制缓冲 | 2×10 行（省 ~57KB） |
| PCM 环 | 64 帧 |
| 已证实的风险 | 用运行时 transform 放大字体会在 C6 上崩（见 `.learnings/LRN-20260924-001`） |

---

## 11. 验收清单

- [ ] `git log --graph` 能看到两条历史在 merge commit 汇合
- [ ] `git tag` 有 `pre-bili-merge` / `v1-legacy-ui-final` / `bili-snapshot-2026-10-18` / `merge-bili-2026-10-31`
- [ ] `scripts/sim-test.sh` → 2/2
- [ ] `scripts/host-test.sh` → 69 passed
- [ ] `scripts/fw-test.sh` → 两套 PASS
- [ ] `scripts/fw-build.sh` → 编译通过，体积符合预期
- [ ] 仓库里搜不到 `/Users/acache`（除 `docs/log` 的历史记录外）
- [ ] `git status` 干净；`tools/lv_offline_sim/build` 已不在索引里
- [ ] README 的构建命令与 `scripts/fw-build.sh` 完全一致
- [ ] `spec/archive/` 顶部有"未实现，仅设计"声明

## 12. 待你拍板

1. **主机目录**：Phase 1 保持 `mac/`（零改动），还是趁合并一次改成 `host/`（约 15 处引用）？
2. **v1 素材**：按建议删除 + tag（推荐），还是移进 `attic/v1/` 留在工作树里？
3. **分区表**：合并后立刻改成 8MB app（推荐，已验证），还是等确定下一步功能规模再改？
4. **远端**：把 GitHub 的 `esp32-ctrl` 改名 `desk-mate` 并快进推送（推荐），还是新建仓库？
