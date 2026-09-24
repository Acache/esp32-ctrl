# Errors

Command failures and integration errors.

---

## [ERR-20260924-001] lvgl_hardware_render

**Logged**: 2026-09-24T00:00:00+08:00
**Priority**: high
**Status**: resolved
**Area**: frontend

### Summary
LVGL transform zoom exhausted the hardware render layer and caused an ESP32 store access fault.

### Error
```
Guru Meditation Error: Core 0 panic'ed (Store access fault)
lv_draw_sw_layer_create -> lv_draw_layer_create -> refr_obj
```

### Context
- Large timer digits used `lv_obj_set_style_transform_zoom`.
- Both desktop simulator variants passed, but the physical ESP32-C6 reboot-looped during first render.
- The generated font initially included `lvgl/lvgl.h`, while Arduino LVGL 8 exposes `lvgl.h`.

### Suggested Fix
Use a glyph-subset font at its native size and set its generated include to `lvgl.h` for this project.

### Metadata
- Reproducible: yes
- Related Files: firmware/desk-mate-app/focus_ui.h, firmware/desk-mate-app/focus_font_68.c

### Resolution
- **Resolved**: 2026-09-24T00:00:00+08:00
- **Notes**: Native 68px digit font eliminates the temporary transformed layer.

---
