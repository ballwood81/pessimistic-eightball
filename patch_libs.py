"""Pre-build: re-apply three one-line library fixes after PlatformIO reinstalls libdeps."""
import os

Import("env")  # noqa: F821

libdeps = os.path.join(env.subst("$PROJECT_LIBDEPS_DIR"), env.subst("$PIOENV"))  # noqa: F821

PATCHES = [
    # Seeed_GFX polls SPIM0's ENDTX, but the display runs blocking transfers on SPIM3 and
    # SPIM0 is the same block as TWIM0 (Wire: touch + RTC). Bus traffic latches the flag
    # and every later flush spins forever.
    (
        "Seeed_GFX/Processors/TFT_eSPI_nRF52840.c",
        "  return NRF_SPIM0->EVENTS_ENDTX; ",
        "  return false; // blocking SPIM3 transfers; SPIM0 is Wire's TWIM0",
    ),
    # IMU sits on Wire1; upstream only switches for the mbed core.
    (
        "Seeed Arduino LSM6DS3/LSM6DS3.cpp",
        "#if defined(TARGET_SEEED_XIAO_NRF52840_SENSE) || defined(TARGET_SEEED_XIAO_NRF52840_SENSE_PLUS)\n",
        "#if defined(TARGET_SEEED_XIAO_NRF52840_SENSE) || defined(TARGET_SEEED_XIAO_NRF52840_SENSE_PLUS) || \\\n"
        "    defined(ARDUINO_Seeed_XIAO_nRF52840_Sense)\n",
    ),
    # Adafruit nRF TWIM repeated-start can block; use a stop between write and read.
    (
        "Seeed Arduino LSM6DS3/LSM6DS3.cpp",
        "            if (Wire.endTransmission(false) != 0) {",
        "            if (Wire.endTransmission() != 0) { // patch_libs: no repeated start",
    ),
]

for rel, old, new in PATCHES:
    path = os.path.join(libdeps, rel)
    if not os.path.exists(path):
        continue
    with open(path, encoding="utf-8") as f:
        text = f.read()
    if new in text:
        continue
    if old not in text:
        print(f"patch_libs: pattern not found in {rel} — upstream changed, check fix")
        continue
    with open(path, "w", encoding="utf-8") as f:
        f.write(text.replace(old, new, 1))
    print(f"patch_libs: patched {rel}")
