# Shadow Wheel Scale Design

## Goal

Double the previous/next timer values from 24px to 48px so the physical wheel cue is easier to perceive on the small AMOLED.

## Design

- Use the existing built-in Montserrat 48 font; add no new font asset.
- Preserve the minute and second column centerlines.
- Preserve the upper and lower shadow-row centerlines by moving their label origins to account for the taller font.
- Reduce idle/active opacity slightly so the 48px neighbors remain subordinate to the 120px current values.
- Keep wheel values, snap movement, touch geometry, timer behavior, and Mac controls unchanged.

## Verification

- Assert a shadow font line height of at least 45px in both simulator versions.
- Run the complete timer/swipe/UI suite.
- Inspect the LVGL 8 hardware render.
- Compile, flash, and verify stable ESP32 boot and BLE reconnection.
