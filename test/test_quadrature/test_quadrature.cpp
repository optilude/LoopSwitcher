#include <unity.h>

#include "ui_framework/quadrature.h"

void setUp() {}
void tearDown() {}

// state = (A << 1) | B. A falling first is +1 (Right); the detent rests at 0b11.
static const uint8_t kForward[] = {0b01, 0b00, 0b10, 0b11};
static const uint8_t kBackward[] = {0b10, 0b00, 0b01, 0b11};

static int feed(QuadratureDecoder& d, const uint8_t* states, int n) {
    int total = 0;
    for (int i = 0; i < n; ++i) total += d.update(states[i]);
    return total;
}

void test_forward_detent_gives_one_positive_step_at_rest() {
    QuadratureDecoder d;
    TEST_ASSERT_EQUAL_INT(0, d.update(0b01));
    TEST_ASSERT_EQUAL_INT(0, d.update(0b00));
    TEST_ASSERT_EQUAL_INT(0, d.update(0b10));
    TEST_ASSERT_EQUAL_INT(1, d.update(0b11));
}

void test_backward_detent_gives_one_negative_step() {
    QuadratureDecoder d;
    TEST_ASSERT_EQUAL_INT(-1, feed(d, kBackward, 4));
}

void test_ten_detents_give_ten_steps() {
    QuadratureDecoder d;
    int total = 0;
    for (int i = 0; i < 10; ++i) total += feed(d, kForward, 4);
    TEST_ASSERT_EQUAL_INT(10, total);
}

void test_contact_bounce_at_rest_gives_no_step() {
    QuadratureDecoder d;
    const uint8_t bounce[] = {0b10, 0b11, 0b10, 0b11, 0b01, 0b11};
    TEST_ASSERT_EQUAL_INT(0, feed(d, bounce, 6));
}

void test_half_turn_and_return_gives_no_step() {
    QuadratureDecoder d;
    const uint8_t halfAndBack[] = {0b01, 0b00, 0b01, 0b11};
    TEST_ASSERT_EQUAL_INT(0, feed(d, halfAndBack, 4));
}

void test_repeated_state_changes_nothing() {
    QuadratureDecoder d;
    d.update(0b01);
    TEST_ASSERT_EQUAL_INT(0, d.update(0b01));
    d.update(0b00);
    d.update(0b10);
    TEST_ASSERT_EQUAL_INT(1, d.update(0b11));
}

void test_one_missed_state_in_a_fast_spin_still_gives_a_step() {
    QuadratureDecoder d;
    const uint8_t skipped[] = {0b00, 0b10, 0b11};  // 0b01 was never seen
    TEST_ASSERT_EQUAL_INT(1, feed(d, skipped, 3));
}

void test_no_drift_after_wiggles() {
    QuadratureDecoder d;
    const uint8_t wiggle[] = {0b01, 0b11, 0b10, 0b11, 0b01, 0b00, 0b01, 0b11};
    TEST_ASSERT_EQUAL_INT(0, feed(d, wiggle, 8));
    TEST_ASSERT_EQUAL_INT(1, feed(d, kForward, 4));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_forward_detent_gives_one_positive_step_at_rest);
    RUN_TEST(test_backward_detent_gives_one_negative_step);
    RUN_TEST(test_ten_detents_give_ten_steps);
    RUN_TEST(test_contact_bounce_at_rest_gives_no_step);
    RUN_TEST(test_half_turn_and_return_gives_no_step);
    RUN_TEST(test_repeated_state_changes_nothing);
    RUN_TEST(test_one_missed_state_in_a_fast_spin_still_gives_a_step);
    RUN_TEST(test_no_drift_after_wiggles);
    return UNITY_END();
}
