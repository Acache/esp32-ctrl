# Timer Visual Hierarchy Design

## Goal

Use the empty area inside the timer ring more confidently while keeping the timer calm, legible, and gesture-first on the 480×480 AMOLED.

## UI/UX

The timer screen uses one vertical reading order:

1. `FOCUS` at the top in 24px amber type.
2. The time centered in a native 104px digit-only font.
3. One contextual instruction below the time:
   - Idle: `ROLL MINUTES / SECONDS`
   - Running: `ONE THING AT A TIME`
   - Paused: `PWR TO RESUME`
   - Finished: `SESSION COMPLETE`
4. A small physical-control legend near the bottom that fades after startup.

The left half of the time remains the minute roll target and the right half remains the seconds target. Vertical editing remains disabled while running. Horizontal page swiping and the Mac control screen are unchanged.

## Implementation

- Generate an uncompressed, 2bpp, digit-and-colon-only 104px font to avoid LVGL transform-layer allocations on the ESP32.
- Reposition the heading, digits, contextual instruction, gesture zones, and temporary control legend in `focus_ui.h`.
- Keep timer state transitions and physical-button behavior unchanged.

## Verification

- Assert the vertical object order and native font size in both LVGL simulator targets.
- Run timer and UI interaction tests.
- Render and inspect a 480×480 screenshot.
- Compile, flash, and verify stable boot and BLE reconnection on the ESP32-C6.
