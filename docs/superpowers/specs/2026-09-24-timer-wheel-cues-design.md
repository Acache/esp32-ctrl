# Timer Wheel Cues Design

## Goal

Make the timer feel like a physical rolling instrument while centering the countdown and enlarging its practical touch area, without changing timer behavior.

## Visual hierarchy

The timer screen is vertically grouped inside the ring:

1. Dynamic state at the top: `FOCUS`, `FOCUSING`, `PAUSED`, or `REST`.
2. Small, low-opacity previous minute and second values.
3. A centered 120px minute, colon, and second row.
4. Small, low-opacity next minute and second values.
5. One contextual instruction: `ROLL MINUTES / SECONDS`, `STAY WITH ONE THING`, `PWR TO CONTINUE`, or `KEY TO RESET`.
6. A temporary physical-button legend near the bottom.

The previous/next values align with their respective minute and second columns. They remain visible at rest and brighten slightly for the active column.

## Interaction

- The left and right invisible gesture surfaces fill nearly the entire inner circle, including the state and instruction regions.
- Vertical motion on the left changes minutes; vertical motion on the right changes seconds.
- A value step updates all three labels in that column and uses a short coordinate-only snap animation. No transform scaling or extra draw layer is used.
- Horizontal page swiping remains available through event bubbling.
- Timer values remain non-editable while running.
- Timer model, physical buttons, presets, BLE, Mac controls, and battery behavior are unchanged.

## Hardware constraints

- Use a native, uncompressed, 2bpp, digit-and-colon-only 120px font.
- Use four additional Montserrat 24 labels for neighboring values.
- Animate label Y coordinates and opacity only; avoid transforms that allocate large LVGL render layers.

## Verification

- Test state text across idle, running, paused, and finished states.
- Test neighbor values and wrap behavior for both columns.
- Test enlarged gesture surfaces from locations outside the large numeral row.
- Preserve horizontal swipe and running edit-lock tests in LVGL 8 and 9.
- Render screenshots, compile, flash, and verify stable boot and BLE reconnection.
