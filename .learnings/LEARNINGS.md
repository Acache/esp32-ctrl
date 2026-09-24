# Learnings

Corrections, insights, and knowledge gaps captured during development.

**Categories**: correction | insight | knowledge_gap | best_practice

---

## [LRN-20260924-001] best_practice

**Logged**: 2026-09-24T00:00:00+08:00
**Priority**: high
**Status**: resolved
**Area**: frontend

### Summary
Use a real subset font for large ESP32 LVGL text instead of style transform zoom.

### Details
Desktop and LVGL 8 simulations tolerated zooming a 48px label, but the ESP32-C6 crashed while LVGL created the transformed draw layer. A 2bpp font containing only timer digits and the colon preserves the large appearance with predictable memory use.

### Suggested Action
Generate digit-only fonts with `lv_font_conv`, compile them into both simulator targets, and verify boot over serial after flashing.

### Metadata
- Source: error
- Related Files: firmware/desk-mate-app/focus_font_68.c, firmware/desk-mate-app/focus_ui.h
- Tags: lvgl, esp32, fonts, memory

### Resolution
- **Resolved**: 2026-09-24T00:00:00+08:00
- **Notes**: Replaced runtime zoom with a 68px 2bpp subset font.

---
