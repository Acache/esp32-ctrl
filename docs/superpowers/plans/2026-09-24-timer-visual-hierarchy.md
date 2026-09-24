# Timer Visual Hierarchy Implementation Plan

> **For agentic workers:** Use `subagent-driven-development` (recommended) or `executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Enlarge and reorder the timer typography into a clear FOCUS → time → instruction hierarchy.

**Architecture:** Keep the existing timer model and gesture handlers. Replace the 68px subset font with a native 104px subset font and adjust only timer-page presentation geometry.

**Tech Stack:** ESP32 Arduino, LVGL 8.4 hardware UI, LVGL 8/9 SDL simulators, CMake/CTest

## Global Constraints

- Preserve minute/second vertical roll gestures.
- Preserve the running-state edit lock.
- Preserve horizontal page swiping, physical controls, battery display, BLE, and Mac controls.
- Do not use runtime transform zoom for large text.

### Task 1: Specify the visual hierarchy in simulation

**Files:**
- Modify: `tools/lv_sim/sim_main.c`
- Test: `tools/lv_sim/sim_main.c`

**Interfaces:**
- Consumes: timer UI object references from `focus_ui.h`
- Produces: layout-order and native-font-size assertions

- [x] Add assertions for title, digits, instruction order and a font line height of at least 75px.
- [x] Run CTest and confirm the new assertions fail.

### Task 2: Build the larger native typography

**Files:**
- Create: `firmware/desk-mate-app/focus_font_104.c`
- Delete: `firmware/desk-mate-app/focus_font_68.c`
- Modify: `firmware/desk-mate-app/focus_ui.h`
- Modify: `tools/lv_sim/CMakeLists.txt`

**Interfaces:**
- Produces: `const lv_font_t focus_font_104`

- [x] Generate an uncompressed 2bpp font containing only `0123456789:`.
- [x] Replace the font declaration and simulator source reference.
- [x] Position FOCUS, time, contextual instruction, gesture zones, wheel hints, and control legend.
- [x] Run CTest and confirm all tests pass in LVGL 8 and 9.

### Task 3: Verify and deliver

**Files:**
- Test: all files above

- [x] Render and visually inspect the timer screenshot.
- [x] Compile the ESP32 firmware and check memory use.
- [x] Flash the connected board and verify stable boot plus BLE reconnection.
- [x] Commit only this feature and its design/plan documentation.
