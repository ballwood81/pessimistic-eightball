#include <cstdint>

#include <unity.h>

#ifdef ARDUINO
#include <Arduino.h>
#endif

#include "eight_ball_logic.h"

#ifdef ARDUINO
#include <Adafruit_TinyUSB.h>
#endif

void test_stationary_and_tilt_do_not_trigger_shake() {
    ShakeDetector detector;

    TEST_ASSERT_FALSE(detector.update({0.0F, 0.0F, 1.0F}, 0));
    TEST_ASSERT_FALSE(detector.update({1.0F, 0.0F, 0.0F}, 50));
    TEST_ASSERT_FALSE(detector.update({0.0F, -1.0F, 0.0F}, 100));
}

void test_three_impacts_with_hysteresis_trigger_once() {
    ShakeDetector detector;

    TEST_ASSERT_FALSE(detector.update({2.0F, 0.0F, 0.0F}, 100));
    TEST_ASSERT_FALSE(detector.update({0.0F, 0.0F, 1.0F}, 160));
    TEST_ASSERT_FALSE(detector.update({2.0F, 0.0F, 0.0F}, 220));
    TEST_ASSERT_FALSE(detector.update({0.0F, 0.0F, 1.0F}, 280));
    TEST_ASSERT_TRUE(detector.update({-2.1F, 0.0F, 0.0F}, 340));
    // latched: further spikes ignored
    TEST_ASSERT_FALSE(detector.update({0.0F, 0.0F, 1.0F}, 360));
    TEST_ASSERT_FALSE(detector.update({2.2F, 0.0F, 0.0F}, 380));

    detector.reset();
    TEST_ASSERT_FALSE(detector.update({2.0F, 0.0F, 0.0F}, 500));
}

void test_impacts_outside_window_do_not_accumulate() {
    ShakeDetector detector;

    TEST_ASSERT_FALSE(detector.update({2.0F, 0.0F, 0.0F}, 100));
    TEST_ASSERT_FALSE(detector.update({0.0F, 0.0F, 1.0F}, 120));
    TEST_ASSERT_FALSE(detector.update({2.0F, 0.0F, 0.0F}, 600));
    TEST_ASSERT_FALSE(detector.update({0.0F, 0.0F, 1.0F}, 620));
    TEST_ASSERT_FALSE(detector.update({2.0F, 0.0F, 0.0F}, 1100));
}

void test_response_picker_never_immediately_repeats() {
    ResponsePicker picker{4};

    const auto first = picker.pick(0);
    const auto second = picker.pick(0);
    const auto third = picker.pick(0);

    TEST_ASSERT_NOT_EQUAL(first, second);
    TEST_ASSERT_NOT_EQUAL(second, third);
    TEST_ASSERT_LESS_THAN_UINT32(4, first);
    TEST_ASSERT_LESS_THAN_UINT32(4, second);
    TEST_ASSERT_LESS_THAN_UINT32(4, third);
}

void test_ball_machine_trigger_reveal_visible_reset() {
    BallTiming timing;
    timing.reveal_ms = 200;
    timing.visible_ms = 300;
    timing.reset_ms = 100;
    timing.input_lock_ms = 50;
    BallMachine machine{timing};

    TEST_ASSERT_TRUE(machine.request_trigger(1000));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(BallState::triggered), static_cast<int>(machine.state()));
    TEST_ASSERT_FALSE(machine.request_trigger(1010));  // busy

    machine.update(1010);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(BallState::revealing), static_cast<int>(machine.state()));
    TEST_ASSERT_FLOAT_WITHIN(0.05F, 0.5F, machine.phase_progress(1110));

    machine.update(1210);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(BallState::visible), static_cast<int>(machine.state()));
    TEST_ASSERT_TRUE(machine.request_dismiss(1220));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(BallState::resetting), static_cast<int>(machine.state()));

    machine.update(1320);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(BallState::idle), static_cast<int>(machine.state()));
}

void test_reroll_while_answer_is_visible() {
    BallTiming timing;
    timing.reveal_ms = 50;
    timing.visible_ms = 5000;
    timing.reset_ms = 40;
    timing.input_lock_ms = 100;
    BallMachine machine{timing};

    TEST_ASSERT_TRUE(machine.request_trigger(0));
    machine.update(1);
    machine.update(60);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(BallState::visible), static_cast<int>(machine.state()));
    TEST_ASSERT_FALSE(machine.request_trigger(80));
    TEST_ASSERT_TRUE(machine.request_trigger(120));
    TEST_ASSERT_EQUAL_INT(static_cast<int>(BallState::triggered), static_cast<int>(machine.state()));
}

void test_ball_machine_auto_timeout_and_input_lock() {
    BallTiming timing;
    timing.reveal_ms = 50;
    timing.visible_ms = 80;
    timing.reset_ms = 40;
    timing.input_lock_ms = 200;
    BallMachine machine{timing};

    TEST_ASSERT_TRUE(machine.request_trigger(0));
    machine.update(1);
    machine.update(60);   // visible
    machine.update(150);  // resetting
    machine.update(200);  // idle
    TEST_ASSERT_EQUAL_INT(static_cast<int>(BallState::idle), static_cast<int>(machine.state()));

    // input lock still active from last_input at 0
    TEST_ASSERT_FALSE(machine.request_trigger(100));
    TEST_ASSERT_TRUE(machine.request_trigger(250));
}

void test_mount_lock_picks_rotation_from_still_gravity() {
    MountLock upright;
    MountLock flipped;
    MountLock flat;
    const Acceleration up = kSeatedAccel;
    const Acceleration down{0.04F, -0.67F, -0.70F};  // 180° in the glass plane
    const Acceleration face_up{0.0F, 0.0F, -1.0F};

    for (std::uint32_t t = 0; t <= 2000; t += 20) {
        upright.add(up, t);
        flipped.add(down, t);
        flat.add(face_up, t);
    }

    TEST_ASSERT_EQUAL_UINT8(0, upright.rotation());
    TEST_ASSERT_EQUAL_UINT8(2, flipped.rotation());
    TEST_ASSERT_EQUAL_UINT8(0, flat.rotation());
}

void test_mount_lock_skips_shakes_and_short_stillness() {
    MountLock lock;
    const Acceleration upright = kSeatedAccel;
    lock.add(upright, 0);
    lock.add(upright, 20);
    lock.add({3.0F, 0.0F, 0.0F}, 40);  // shake, dropped
    TEST_ASSERT_FALSE(lock.still_ms >= 2000);
    TEST_ASSERT_EQUAL_UINT8(0, lock.rotation());  // not enough quiet time

    for (std::uint32_t t = 60; t <= 2060; t += 20) {
        const Acceleration sample = (t == 500) ? Acceleration{2.5F, 0.0F, 0.0F} : upright;
        lock.add(sample, t);
    }
    TEST_ASSERT_EQUAL_UINT8(0, lock.rotation());
    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(2000, lock.still_ms);
}

void test_face_follows_a_slow_turn_each_second() {
    const Acceleration upright = kSeatedAccel;
    const Acceleration inverted{0.04F, -0.67F, -0.70F};
    const Acceleration face_up{0.0F, 0.0F, -1.0F};
    const Acceleration sideways{0.80F, 0.0F, -0.50F};

    FaceOrient face;
    std::uint8_t out = 9;
    TEST_ASSERT_TRUE(face.update(inverted, 0, 0, out));
    TEST_ASSERT_EQUAL_UINT8(2, out);
    TEST_ASSERT_FALSE(face.update(upright, 500, 2, out));
    TEST_ASSERT_TRUE(face.update(upright, 1000, 2, out));
    TEST_ASSERT_EQUAL_UINT8(0, out);
    TEST_ASSERT_FALSE(face.update({3.0F, 0.0F, 0.0F}, 2000, 0, out));
    TEST_ASSERT_FALSE(face.update(face_up, 3000, 0, out));
    TEST_ASSERT_FALSE(face.update(sideways, 4000, 0, out));
}

void run_all_tests() {
    RUN_TEST(test_stationary_and_tilt_do_not_trigger_shake);
    RUN_TEST(test_three_impacts_with_hysteresis_trigger_once);
    RUN_TEST(test_impacts_outside_window_do_not_accumulate);
    RUN_TEST(test_response_picker_never_immediately_repeats);
    RUN_TEST(test_ball_machine_trigger_reveal_visible_reset);
    RUN_TEST(test_reroll_while_answer_is_visible);
    RUN_TEST(test_ball_machine_auto_timeout_and_input_lock);
    RUN_TEST(test_mount_lock_picks_rotation_from_still_gravity);
    RUN_TEST(test_mount_lock_skips_shakes_and_short_stillness);
    RUN_TEST(test_face_follows_a_slow_turn_each_second);
}

#ifdef ARDUINO
void setup() {
    delay(2000);
    UNITY_BEGIN();
    run_all_tests();
    UNITY_END();
}

void loop() {}
#else
int main(int, char**) {
    UNITY_BEGIN();
    run_all_tests();
    return UNITY_END();
}
#endif
