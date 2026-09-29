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

void run_all_tests() {
    RUN_TEST(test_stationary_and_tilt_do_not_trigger_shake);
    RUN_TEST(test_three_impacts_with_hysteresis_trigger_once);
    RUN_TEST(test_impacts_outside_window_do_not_accumulate);
    RUN_TEST(test_response_picker_never_immediately_repeats);
    RUN_TEST(test_ball_machine_trigger_reveal_visible_reset);
    RUN_TEST(test_reroll_while_answer_is_visible);
    RUN_TEST(test_ball_machine_auto_timeout_and_input_lock);
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
