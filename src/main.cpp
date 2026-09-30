#include <Adafruit_TinyUSB.h>
#include <Arduino.h>
#include <LSM6DS3.h>
#include <I2C_BM8563.h>
#include <Wire.h>
#include <cmath>
#include <cstdint>
#include <cstdio>

#include "driver.h"
#include <lvgl.h>
#include "lv_xiao_round_screen.h"

#include "eight_ball_logic.h"
#include "responses.h"

namespace {

constexpr std::uint32_t kImuIntervalMs = 20;
constexpr std::int16_t kSafeTextWidth = 168;
constexpr std::int16_t kRevealDriftPx = 18;
constexpr std::uint32_t kAnimMs = 700;

LSM6DS3 imu{I2C_MODE, 0x6A};
I2C_BM8563 rtc{I2C_BM8563_DEFAULT_ADDRESS, Wire};
bool rtc_ok = false;
void refresh_clock();
ShakeDetector shake_detector;
FaceOrient face_orient;
bool imu_ok = false;
ResponsePicker response_picker{kResponseCount};
BallMachine ball;

const Response* current_response = nullptr;
BallState previous_state = BallState::idle;
std::uint32_t last_imu_ms = 0;

lv_obj_t* scr = nullptr;
lv_obj_t* badge = nullptr;
lv_obj_t* eight_mark = nullptr;
lv_obj_t* window = nullptr;
lv_obj_t* eye_l = nullptr;
lv_obj_t* eye_r = nullptr;
lv_obj_t* pupil_l = nullptr;
lv_obj_t* pupil_r = nullptr;
lv_obj_t* mouth = nullptr;
lv_obj_t* clock_label = nullptr;
char clock_text[9] = "--:--:--";
lv_obj_t* kind_label = nullptr;
lv_obj_t* body_label = nullptr;

constexpr std::int16_t kEyeOpen = 28;
constexpr std::int16_t kEyeShut = 5;

lv_color_t kind_color(const ResponseKind kind) {
    switch (kind) {
        case ResponseKind::yes:
            return lv_color_hex(0x3d9b6e);
        case ResponseKind::no:
            return lv_color_hex(0xc44b4b);
        case ResponseKind::maybe:
            return lv_color_hex(0xc4843a);
        case ResponseKind::ominous:
            return lv_color_hex(0x9a5fb5);
    }
    return lv_color_hex(0xd8d0c4);
}

const char* kind_caption(const ResponseKind kind) {
    switch (kind) {
        case ResponseKind::yes:
            return "YES, SOMEHOW";
        case ResponseKind::no:
            return "ABSOLUTELY NOT";
        case ResponseKind::maybe:
            return "ANNOYINGLY MAYBE";
        case ResponseKind::ominous:
            return "THE VOID SAYS";
    }
    return "MYSTIC ERROR";
}

void set_response_hidden(const bool hidden) {
    if (hidden) {
        lv_obj_add_flag(kind_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(body_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(kind_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(body_label, LV_OBJ_FLAG_HIDDEN);
    }
}

void set_face_hidden(const bool hidden) {
    const lv_obj_t* parts[] = {badge, eye_l, eye_r, pupil_l, pupil_r, mouth, clock_label};
    for (const auto* part : parts) {
        if (hidden) {
            lv_obj_add_flag(const_cast<lv_obj_t*>(part), LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(const_cast<lv_obj_t*>(part), LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void tick_face(const std::uint32_t now_ms) {
    if (ball.state() != BallState::idle) {
        return;
    }
    static std::uint32_t last_tick_ms = 0;
    if (now_ms - last_tick_ms < 80U) {
        return;
    }
    last_tick_ms = now_ms;

    const bool blink = (now_ms % 4300U) < 180U;
    const auto eye_h = static_cast<std::int16_t>(blink ? kEyeShut : kEyeOpen);
    lv_obj_set_height(eye_l, eye_h);
    lv_obj_set_height(eye_r, eye_h);

    constexpr std::int8_t looks[] = {-6, -3, 0, 4, 7, 2, -2};
    const auto look = looks[(now_ms / 700U) % 7U];
    if (blink) {
        lv_obj_add_flag(pupil_l, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pupil_r, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(pupil_l, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(pupil_r, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(pupil_l, LV_ALIGN_CENTER, look, 0);
        lv_obj_align(pupil_r, LV_ALIGN_CENTER, look, 1);
    }

    const auto wobble = static_cast<std::int16_t>((now_ms / 550U) % 3U);
    lv_obj_align(mouth, LV_ALIGN_CENTER, 0, static_cast<std::int16_t>(46 + wobble));

    static std::uint32_t last_clock_ms = 0;
    if (!rtc_ok || now_ms - last_clock_ms < 1000U) {
        return;
    }
    last_clock_ms = now_ms;

    refresh_clock();
}

struct ClockStamp {
    int year = 0;
    int month = 0;
    int day = 0;
    int hours = 0;
    int minutes = 0;
    int seconds = 0;
};

int bcd_byte(const uint8_t value) {
    return static_cast<int>((value >> 4) * 10 + (value & 0x0F));
}

bool build_stamp(ClockStamp& out) {
    const char* date = __DATE__;
    const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    int month = 0;
    for (int i = 0; i < 12; ++i) {
        if (date[0] == months[i * 3] && date[1] == months[i * 3 + 1] && date[2] == months[i * 3 + 2]) {
            month = i + 1;
            break;
        }
    }
    if (month == 0) {
        return false;
    }
    out.month = month;
    out.day = (date[4] == ' ' ? 0 : date[4] - '0') * 10 + (date[5] - '0');
    out.year = (date[7] - '0') * 1000 + (date[8] - '0') * 100 + (date[9] - '0') * 10 + (date[10] - '0');
    out.hours = (__TIME__[0] - '0') * 10 + (__TIME__[1] - '0');
    out.minutes = (__TIME__[3] - '0') * 10 + (__TIME__[4] - '0');
    out.seconds = (__TIME__[6] - '0') * 10 + (__TIME__[7] - '0');
    return true;
}

bool stamp_sane(const ClockStamp& stamp) {
    return stamp.year >= 2026 && stamp.year <= 2035 && stamp.month >= 1 && stamp.month <= 12
        && stamp.day >= 1 && stamp.day <= 31 && stamp.hours <= 23 && stamp.minutes <= 59
        && stamp.seconds <= 59;
}

bool stamp_before(const ClockStamp& rtc, const ClockStamp& built) {
    if (rtc.year != built.year) {
        return rtc.year < built.year;
    }
    if (rtc.month != built.month) {
        return rtc.month < built.month;
    }
    if (rtc.day != built.day) {
        return rtc.day < built.day;
    }
    if (rtc.hours != built.hours) {
        return rtc.hours < built.hours;
    }
    if (rtc.minutes != built.minutes) {
        return rtc.minutes < built.minutes;
    }
    return rtc.seconds < built.seconds;
}

// nRF TWIM repeated-start can wait forever. Use a full stop between write and read.
bool read_stamp(ClockStamp& out, bool* voltage_low) {
    Wire.beginTransmission(I2C_BM8563_DEFAULT_ADDRESS);
    Wire.write(0x02);
    if (Wire.endTransmission() != 0) {
        return false;
    }
    if (Wire.requestFrom(static_cast<uint8_t>(I2C_BM8563_DEFAULT_ADDRESS), static_cast<uint8_t>(7)) != 7) {
        return false;
    }
    uint8_t raw[7];
    for (uint8_t& byte : raw) {
        byte = static_cast<uint8_t>(Wire.read());
    }
    if (voltage_low != nullptr) {
        *voltage_low = (raw[0] & 0x80) != 0;
    }
    out.seconds = bcd_byte(raw[0] & 0x7F);
    out.minutes = bcd_byte(raw[1] & 0x7F);
    out.hours = bcd_byte(raw[2] & 0x3F);
    out.day = bcd_byte(raw[3] & 0x3F);
    out.month = bcd_byte(raw[5] & 0x1F);
    out.year = ((raw[5] & 0x80) != 0 ? 1900 : 2000) + bcd_byte(raw[6]);
    return out.hours <= 23 && out.minutes <= 59;
}

// Three bytes only. A 7-byte read on this shared bus fails once touch polls, and the label stays put.
bool read_clock(int& hours, int& minutes, int& seconds) {
    Wire.beginTransmission(I2C_BM8563_DEFAULT_ADDRESS);
    Wire.write(0x02);
    if (Wire.endTransmission() != 0) {
        return false;
    }
    if (Wire.requestFrom(static_cast<uint8_t>(I2C_BM8563_DEFAULT_ADDRESS), static_cast<uint8_t>(3)) != 3) {
        return false;
    }
    const int raw_seconds = bcd_byte(static_cast<uint8_t>(Wire.read()) & 0x7F);
    const int raw_minutes = bcd_byte(static_cast<uint8_t>(Wire.read()) & 0x7F);
    const int raw_hours = bcd_byte(static_cast<uint8_t>(Wire.read()) & 0x3F);
    if (raw_hours > 23 || raw_minutes > 59 || raw_seconds > 59) {
        return false;
    }
    hours = raw_hours;
    minutes = raw_minutes;
    seconds = raw_seconds;
    return true;
}

void apply_stamp(const ClockStamp& stamp) {
    I2C_BM8563_DateTypeDef date{};
    date.year = static_cast<int16_t>(stamp.year);
    date.month = static_cast<int8_t>(stamp.month);
    date.date = static_cast<int8_t>(stamp.day);
    date.weekDay = 0;
    I2C_BM8563_TimeTypeDef time{};
    time.hours = static_cast<int8_t>(stamp.hours);
    time.minutes = static_cast<int8_t>(stamp.minutes);
    time.seconds = static_cast<int8_t>(stamp.seconds);
    rtc.WriteReg(0x00, 0x20);  // STOP=1 while the registers are written
    rtc.setDate(&date);
    rtc.setTime(&time);
    rtc.WriteReg(0x00, 0x00);  // STOP=0, oscillator runs
}

void refresh_clock() {
    int hours = 0;
    int minutes = 0;
    int seconds = 0;
    if (!read_clock(hours, minutes, seconds)) {
        return;
    }
    std::snprintf(clock_text, sizeof(clock_text), "%02d:%02d:%02d", hours, minutes, seconds);
    if (clock_label != nullptr) {
        lv_label_set_text(clock_label, clock_text);
    }
}

void place_response(const std::int16_t y_shift) {
    // Keep text opaque. A fade-to-zero on reroll left the labels invisible.
    lv_obj_set_style_text_opa(kind_label, LV_OPA_COVER, 0);
    lv_obj_set_style_text_opa(body_label, LV_OPA_COVER, 0);
    lv_obj_align(kind_label, LV_ALIGN_TOP_MID, 0, static_cast<std::int16_t>(78 + y_shift));
    lv_obj_align(body_label, LV_ALIGN_TOP_MID, 0, static_cast<std::int16_t>(108 + y_shift));
    lv_obj_invalidate(kind_label);
    lv_obj_invalidate(body_label);
}

void apply_reveal_visual(const float progress) {
    const auto y_off = static_cast<std::int16_t>(kRevealDriftPx * (1.0F - progress));
    place_response(y_off);
}

void apply_reset_visual(const float progress) {
    const auto opa = static_cast<lv_opa_t>((1.0F - progress) * LV_OPA_COVER);
    lv_obj_set_style_text_opa(kind_label, opa, 0);
    lv_obj_set_style_text_opa(body_label, opa, 0);
}

void show_idle() {
    current_response = nullptr;
    set_response_hidden(true);
    set_face_hidden(false);
    lv_obj_set_style_border_color(window, lv_color_hex(0xf4f4f4), 0);
    shake_detector.reset();
}

void begin_reveal() {
    const auto random_value = static_cast<std::uint32_t>(random(0x7FFFFFFF));
    current_response = &kResponses[response_picker.pick(random_value)];

    set_face_hidden(true);
    set_response_hidden(false);

    const auto accent = kind_color(current_response->kind);
    lv_obj_set_style_border_color(window, accent, 0);
    lv_obj_set_style_text_color(kind_label, accent, 0);
    lv_label_set_text(kind_label, kind_caption(current_response->kind));
    lv_label_set_text(body_label, current_response->text);
    lv_obj_move_foreground(kind_label);
    lv_obj_move_foreground(body_label);
    place_response(kRevealDriftPx);
}

void advance_from_triggered(const std::uint32_t now) {
    ball.update(now);  // TRIGGERED → REVEALING immediately
}

void on_screen_event(lv_event_t* event) {
    if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
        return;
    }
    const auto now = millis();
    // Tap always re-rolls. Same habit as shaking a real 8-ball.
    if (ball.request_trigger(now)) {
        begin_reveal();
        advance_from_triggered(now);
    }
}

lv_obj_t* make_eye(lv_obj_t* parent, const std::int16_t x_ofs) {
    auto* eye = lv_obj_create(parent);
    lv_obj_set_size(eye, 36, kEyeOpen);
    lv_obj_set_style_radius(eye, 10, 0);
    lv_obj_set_style_bg_color(eye, lv_color_hex(0xf4f1ea), 0);
    lv_obj_set_style_bg_opa(eye, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(eye, 0, 0);
    lv_obj_set_style_pad_all(eye, 0, 0);
    lv_obj_clear_flag(eye, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(eye, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(eye, LV_ALIGN_CENTER, x_ofs, -8);
    return eye;
}

lv_obj_t* make_pupil(lv_obj_t* eye) {
    auto* pupil = lv_obj_create(eye);
    lv_obj_set_size(pupil, 12, 12);
    lv_obj_set_style_radius(pupil, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(pupil, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(pupil, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pupil, 0, 0);
    lv_obj_clear_flag(pupil, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(pupil, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(pupil, LV_ALIGN_CENTER, 0, 0);
    return pupil;
}

void build_ui() {
    scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x050505), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_CLICKABLE);

    window = lv_obj_create(scr);
    lv_obj_set_size(window, 240, 240);
    lv_obj_center(window);
    lv_obj_set_style_radius(window, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(window, lv_color_hex(0x071018), 0);
    lv_obj_set_style_bg_opa(window, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(window, 6, 0);
    lv_obj_set_style_border_color(window, lv_color_hex(0xf4f4f4), 0);
    lv_obj_set_style_pad_all(window, 0, 0);
    lv_obj_clear_flag(window, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(window, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(window, on_screen_event, LV_EVENT_CLICKED, nullptr);

    // 8 sits on the forehead, between the eyes. Smaller than the old shell badge.
    badge = lv_obj_create(window);
    lv_obj_set_size(badge, 40, 40);
    lv_obj_align(badge, LV_ALIGN_CENTER, 0, -54);
    lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0xf7f7f7), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(badge, 0, 0);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_CLICKABLE);

    eight_mark = lv_label_create(badge);
    lv_label_set_text(eight_mark, "8");
    lv_obj_set_style_text_font(eight_mark, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(eight_mark, lv_color_hex(0x111111), 0);
    lv_obj_align(eight_mark, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(eight_mark, LV_OBJ_FLAG_CLICKABLE);

    eye_l = make_eye(window, -46);
    eye_r = make_eye(window, 46);
    pupil_l = make_pupil(eye_l);
    pupil_r = make_pupil(eye_r);

    // Frown: arc through 12 o'clock (bulge up, corners down).
    mouth = lv_arc_create(window);
    lv_obj_set_size(mouth, 88, 88);
    lv_arc_set_bg_angles(mouth, 220, 320);
    lv_arc_set_value(mouth, 0);
    lv_obj_remove_style(mouth, nullptr, LV_PART_KNOB);
    lv_obj_set_style_arc_width(mouth, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_color(mouth, lv_color_hex(0xd06a28), LV_PART_MAIN);
    lv_obj_set_style_arc_width(mouth, 0, LV_PART_INDICATOR);
    lv_obj_clear_flag(mouth, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(mouth, LV_ALIGN_CENTER, 0, 46);

    clock_label = lv_label_create(window);
    lv_label_set_text(clock_label, clock_text);
    lv_obj_set_style_text_font(clock_label, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(clock_label, lv_color_hex(0x8a8478), 0);
    lv_obj_align(clock_label, LV_ALIGN_CENTER, 0, 68);
    lv_obj_clear_flag(clock_label, LV_OBJ_FLAG_CLICKABLE);

    kind_label = lv_label_create(window);
    lv_label_set_text(kind_label, "");
    lv_obj_set_width(kind_label, kSafeTextWidth);
    lv_obj_set_style_text_align(kind_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(kind_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_letter_space(kind_label, 1, 0);
    lv_obj_align(kind_label, LV_ALIGN_TOP_MID, 0, 78);
    lv_obj_clear_flag(kind_label, LV_OBJ_FLAG_CLICKABLE);

    body_label = lv_label_create(window);
    lv_label_set_text(body_label, "");
    lv_obj_set_width(body_label, kSafeTextWidth);
    lv_label_set_long_mode(body_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(body_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(body_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(body_label, lv_color_hex(0xe6e0d6), 0);
    lv_obj_set_style_text_line_space(body_label, 3, 0);
    lv_obj_align(body_label, LV_ALIGN_TOP_MID, 0, 108);
    lv_obj_clear_flag(body_label, LV_OBJ_FLAG_CLICKABLE);

    set_response_hidden(true);
}

void on_state_entered(const BallState state) {
    switch (state) {
        case BallState::idle:
            show_idle();
            break;
        case BallState::triggered:
        case BallState::revealing:
            // begin_reveal already called from trigger paths
            break;
        case BallState::visible:
            apply_reveal_visual(1.0F);
            shake_detector.reset();  // answer is up — next shake can count
            break;
        case BallState::resetting:
            break;
    }
}

Acceleration read_imu() {
    return {imu.readFloatAccelX(), imu.readFloatAccelY(), imu.readFloatAccelZ()};
}

void apply_screen_rotation(const std::uint8_t rot) {
    if (rot == screen_rotation) {
        return;
    }
    screen_rotation = rot;
    tft.setRotation(rot);
    if (scr != nullptr) {
        lv_obj_invalidate(scr);
        lv_refr_now(nullptr);
    }
    Serial.print("face rot=");
    Serial.println(rot);
}

// Blocks until ~2s of quiet gravity, or 10s. Shakes are skipped. Fallback is rotation 0.
std::uint8_t lock_screen_rotation() {
    MountLock lock;
    const MountLockConfig config;
    const auto started = millis();
    while (millis() - started < config.deadline_ms) {
        if (lock.add(read_imu(), millis(), config)) {
            break;
        }
        delay(20);
    }
    const auto rot = lock.rotation(config);
    const float inv = lock.still_ms == 0 ? 0.0F : 1.0F / static_cast<float>(lock.still_ms);
    Serial.print("mount still_ms=");
    Serial.print(lock.still_ms);
    Serial.print(" ax=");
    Serial.print(lock.sum_x * inv, 2);
    Serial.print(" ay=");
    Serial.print(lock.sum_y * inv, 2);
    Serial.print(" az=");
    Serial.print(lock.sum_z * inv, 2);
    Serial.print(" rot=");
    Serial.println(rot);
    return rot;
}

}  // namespace

void setup() {
    Serial.begin(115200);

    // screen_rotation is latched in lv_xiao_disp_init, so the IMU has to run first.
    // Rail stays off and Wire1 waits forever if this pin is low.
    pinMode(PIN_LSM6DS3TR_C_POWER, OUTPUT);
    digitalWrite(PIN_LSM6DS3TR_C_POWER, HIGH);
    delay(20);
    imu_ok = imu.begin() == 0;
    if (!imu_ok) {
        Serial.println("IMU NOT FOUND");
        screen_rotation = 0;
    } else {
        Serial.println("IMU ok");
        screen_rotation = lock_screen_rotation();
    }

    lv_init();
    lv_xiao_disp_init();
    lv_xiao_touch_init();
    build_ui();
    show_idle();
    for (std::uint8_t frame = 0; frame < 4; ++frame) {
        lv_timer_handler();
        delay(10);
    }

    // Touch and the BM8563 share Wire. IMU stays on Wire1.
    // ponytail: seed when VL is set, the date is garbage, or this firmware is newer
    // than the RTC. A later reboot keeps coin-cell time. Ceiling: no clock newer than
    // the build survives a dead cell; flash again to restamp.
    ClockStamp running{};
    ClockStamp built{};
    bool voltage_low = false;
    rtc_ok = read_stamp(running, &voltage_low);
    if (rtc_ok) {
        rtc.begin();
        if (build_stamp(built)
            && (voltage_low || !stamp_sane(running) || stamp_before(running, built))) {
            apply_stamp(built);
            Serial.println("RTC seeded from build time");
        }
        refresh_clock();
        Serial.println("RTC ok");
    } else {
        Serial.println("RTC NOT FOUND");
    }

    const auto seed = static_cast<unsigned long>(
        micros() ^ static_cast<std::uint32_t>(std::fabs(imu.readFloatAccelX()) * 100000.0F));
    randomSeed(seed);

    if (imu_ok) {
        face_orient.arm(millis());
    }

    Serial.println("pessimistic eight-ball ready");
}

void loop() {
    const auto now_ms = millis();
    ball.update(now_ms);
    const auto state = ball.state();

    if (state != previous_state) {
        previous_state = state;
        on_state_entered(state);
    }

    if (state == BallState::idle) {
        tick_face(now_ms);
    } else if (state == BallState::revealing) {
        apply_reveal_visual(ball.phase_progress(now_ms));
    } else if (state == BallState::resetting) {
        apply_reset_visual(ball.phase_progress(now_ms));
    }

    if (ball.accepts_shake() && now_ms - last_imu_ms >= kImuIntervalMs) {
        last_imu_ms = now_ms;
        if (shake_detector.update(read_imu(), now_ms)) {
            if (ball.request_trigger(now_ms)) {
                begin_reveal();
                advance_from_triggered(now_ms);
            }
        }
    }

    if (imu_ok && face_orient.wants_sample(now_ms)) {
        const Acceleration sample = read_imu();
        std::uint8_t rot = screen_rotation;
        const bool turn = face_orient.update(sample, now_ms, screen_rotation, rot);
        Serial.print("grav ax=");
        Serial.print(sample.x_g, 2);
        Serial.print(" ay=");
        Serial.print(sample.y_g, 2);
        Serial.print(" az=");
        Serial.print(sample.z_g, 2);
        Serial.print(" up=");
        Serial.print(screen_up_g(sample), 2);
        Serial.print(" rot=");
        Serial.println(turn ? rot : screen_rotation);
        if (turn) {
            apply_screen_rotation(rot);
        }
    }

    static std::uint32_t last_beat_ms = 0;
    if (now_ms - last_beat_ms >= 2000U) {
        last_beat_ms = now_ms;
        Serial.print("alive state=");
        Serial.println(static_cast<int>(state));
    }

    lv_timer_handler();
    delay(5);  // Seeed Round Display examples — keeps LVGL tick healthy
    (void)kAnimMs;
}
