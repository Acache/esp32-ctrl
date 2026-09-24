# Shadow Wheel Scale Implementation Plan

> **For agentic workers:** Use `subagent-driven-development` (recommended) or `executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Enlarge both previous/next timer rows from 24px to 48px without disturbing the centered 120px current time.

**Architecture:** Reuse the compiled Montserrat 48 font and adjust only shadow-label geometry, opacity, and snap baselines. The timer model and gesture handling remain unchanged.

**Tech Stack:** ESP32 Arduino, LVGL 8.4, LVGL 8/9 SDL simulators, CMake/CTest

## Global Constraints

- Shadow values use 48px type.
- Current values remain 120px and centered.
- Gesture areas and timer behavior do not change.
- No additional font asset or transform scaling.

### Task 1: Specify and implement shadow scale

**Files:**
- Modify: `tools/lv_sim/sim_main.c`
- Modify: `firmware/desk-mate-app/focus_ui.h`

- [x] Add a failing assertion for a shadow font line height of at least 45px.
- [x] Change both shadow rows to Montserrat 48.
- [x] Preserve row and column centerlines and tune opacity.
- [x] Update snap-animation baselines.
- [x] Run all simulator tests.

### Task 2: Hardware delivery

**Files:**
- Test: all files above

- [x] Inspect the LVGL 8 screenshot.
- [x] Compile and flash the ESP32-C6.
- [x] Verify stable boot and BLE reconnection.
- [x] Commit only this visual refinement and documentation.
