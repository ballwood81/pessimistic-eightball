# Pessimistic Eight Ball

A sarcastic fortune teller for the Seeed Studio XIAO nRF52840 Sense and Round Display for XIAO.

Shake it deliberately and the idle face gives way to a 900 ms mystic reveal. One of 48 yes, no, maybe, or ominous predictions remains visible for 15 seconds before the face returns.

Parametric enclosure source, printable parts, test coupons, and assembly notes
are in [cad/](cad/README.md).

## Hardware

- Seeed Studio XIAO nRF52840 Sense
- Seeed Studio Round Display for XIAO, 240 x 240 GC9A01
- USB data cable connected to the **XIAO USB-C port** for programming

The accelerometer is the XIAO Sense's onboard LSM6DS3TR-C. The Round Display provides the LCD, touch controller, RTC, and microSD slot.

The microSD card is not used. It is unnecessary for procedural graphics and static text, and Seeed documents support for FAT cards only up to 32 GB; the installed 64 GB card is outside that specification.

## Build and upload

Install the PlatformIO extension in VS Code, then run:

```powershell
pio run -e xiao_nrf52840_sense
pio run -e xiao_nrf52840_sense --target upload
```

If no USB serial device appears, ensure the cable is connected to the XIAO rather than the Round Display's power/charging USB-C port. Double-press the XIAO reset button to enter its UF2 bootloader, then retry upload.

## Tests

Logic tests cover shake discrimination, hit-window expiry, response repetition, reveal timing, busy-state lockout, and `millis()` rollover.

Run them on the attached XIAO:

```powershell
pio test -e xiao_tests
```

The `native` test environment is also configured, but requires `gcc` and `g++` on `PATH`:

```powershell
pio test -e native
```

## Tuning

Shake sensitivity is controlled by `ShakeConfig` in [include/eight_ball_logic.h](include/eight_ball_logic.h). Defaults require three acceleration deviations of at least 0.65 g within 400 ms. This rejects ordinary tilt because it uses acceleration magnitude rather than any single axis.

Predictions live in [include/responses.h](include/responses.h). Keep additions fairly short so they fit within six 20-character lines on the round display.