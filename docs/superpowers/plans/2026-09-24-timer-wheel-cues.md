# Timer Wheel Cues Implementation Plan

> **For agentic workers:** Use `subagent-driven-development` (recommended) or `executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a centered, state-aware timer with persistent physical-wheel cues and larger minute/second gesture surfaces.

**Architecture:** Split the current time into minute, colon, and second labels so each field can roll independently. Derive current and neighboring values from the unchanged `FocusTimer`, and animate only Y coordinates for low-cost snap feedback.

**Tech Stack:** ESP32 Arduino, LVGL 8.4, LVGL 8/9 SDL simulators, CMake/CTest

## Global Constraints

- Do not change timer semantics or physical-button actions.
- Preserve the running-state edit lock and horizontal page swiping.
- Use native font rendering with no transform zoom.
- Keep the Mac screen and BLE implementation unchanged.

### Task 1: Define state and wheel expectations

**Files:**
- Modify: `tools/lv_sim/sim_main.c`

- [x] Add failing assertions for separate current fields, persistent neighboring values, centered geometry, dynamic state text, and expanded gesture reach.
- [x] Run LVGL 8 and 9 tests to confirm failure.

### Task 2: Implement native wheel presentation

**Files:**
- Create: `firmware/desk-mate-app/focus_font_120.c`
- Delete: `firmware/desk-mate-app/focus_font_104.c`
- Modify: `firmware/desk-mate-app/focus_ui.h`
- Modify: `tools/lv_sim/CMakeLists.txt`

- [x] Generate the uncompressed 2bpp 120px timer font.
- [x] Split minute, colon, and second current labels.
- [x] Add persistent previous/next labels for both fields.
- [x] Add dynamic state and contextual instruction text.
- [x] Enlarge both invisible gesture surfaces.
- [x] Add coordinate-only snap feedback.
- [x] Run the full simulator suite.

### Task 3: Hardware verification and delivery

**Files:**
- Test: all files above

- [x] Render and inspect idle and running timer screenshots.
- [x] Compile and flash the ESP32-C6 firmware.
- [x] Verify stable boot, memory, bonding, and BLE media notifications.
- [x] Commit only the wheel UI and its documentation.
