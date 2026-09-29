# Project status — 2026-09-29 (LVGL polish)

## Diagnosis (pre-change)

| Symptom | Likely cause |
| --- | --- |
| Dirty / flicker | `fillScreen` + full idle-face redraw every 80 ms; no retained scene |
| Tear-ish lines earlier | wrong combo 666 (fixed) then heavy full-frame SPI |
| IMU false/miss | shake counted every sample above threshold with no hysteresis/cooldown; no shared lock vs touch |

## Shipped

- LVGL 8.3.11 + Seeed Round Display (`lv_xiao_*`) + Seeed_GFX combo **501**
- Persistent UI objects; fade + upward drift via `text_opa` / `y` (no full wipe)
- CHSC6X touch via Round Display LVGL indev; tap reveal / tap dismiss
- Shake: enter/exit hysteresis + hit window + cooldown; gated by `BallMachine`
- State: IDLE → TRIGGERED → REVEALING → VISIBLE → RESETTING → IDLE
- Responses unchanged
- Built + DFU flashed (RAM ~20%, flash ~28%)

## Physical test still needed

- Tap feel / debounce on glass
- Shake sensitivity on desk vs hand
- Reveal readability for longest ominous lines
- Auto-timeout vs tap-dismiss race
