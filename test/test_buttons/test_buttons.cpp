#include <unity.h>

#include "ui_framework/buttons.h"
#include "ui_framework/event_queue.h"

void setUp() {}
void tearDown() {}

// Feeds `pressed` every 5 ms from `from` to `to` inclusive; returns the last non-None result.
static Button::Result hold(Button& b, bool pressed, uint32_t from, uint32_t to) {
    Button::Result last = Button::Result::None;
    for (uint32_t t = from; t <= to; t += 5) {
        Button::Result r = b.update(pressed, t);
        if (r != Button::Result::None) last = r;
    }
    return last;
}

void test_bounce_shorter_than_debounce_gives_nothing() {
    Button b;
    TEST_ASSERT_EQUAL(Button::Result::None, hold(b, true, 0, 10));
    TEST_ASSERT_EQUAL(Button::Result::None, hold(b, false, 15, 100));
}

void test_short_press_fires_on_release() {
    Button b;
    TEST_ASSERT_EQUAL(Button::Result::None, hold(b, true, 0, 200));
    TEST_ASSERT_EQUAL(Button::Result::Short, hold(b, false, 205, 260));
}

void test_long_press_fires_once_at_the_threshold() {
    Button b;
    TEST_ASSERT_EQUAL(Button::Result::None, hold(b, true, 0, 595));
    TEST_ASSERT_EQUAL(Button::Result::Long, b.update(true, 600));
    TEST_ASSERT_EQUAL(Button::Result::None, hold(b, true, 605, 2000));
}

void test_no_short_press_after_a_long_press() {
    Button b;
    hold(b, true, 0, 700);
    TEST_ASSERT_EQUAL(Button::Result::None, hold(b, false, 705, 800));
}

void test_press_can_repeat_after_release() {
    Button b;
    hold(b, true, 0, 100);
    TEST_ASSERT_EQUAL(Button::Result::Short, hold(b, false, 105, 200));
    hold(b, true, 300, 400);
    TEST_ASSERT_EQUAL(Button::Result::Short, hold(b, false, 405, 500));
}

void test_millis_wraparound_does_not_break_timing() {
    Button b;
    const uint32_t start = 0xFFFFFF00u;
    TEST_ASSERT_EQUAL(Button::Result::None, hold(b, true, start, start + 100));
    TEST_ASSERT_EQUAL(Button::Result::Short, hold(b, false, start + 105, start + 200));
}

void test_queue_is_first_in_first_out_and_empty_pops_none() {
    EventQueue q;
    TEST_ASSERT_EQUAL(Event::None, q.pop());
    q.push(Event::Left);
    q.push(Event::Select);
    TEST_ASSERT_EQUAL(Event::Left, q.pop());
    TEST_ASSERT_EQUAL(Event::Select, q.pop());
    TEST_ASSERT_EQUAL(Event::None, q.pop());
}

void test_queue_overflow_drops_the_oldest() {
    EventQueue q;
    q.push(Event::Mode);  // oldest, to be dropped
    for (int i = 0; i < EventQueue::kCapacity; ++i) q.push(Event::Left);
    for (int i = 0; i < EventQueue::kCapacity; ++i) TEST_ASSERT_EQUAL(Event::Left, q.pop());
    TEST_ASSERT_EQUAL(Event::None, q.pop());
}

void test_buttons_map_to_events() {
    Buttons buttons;
    EventQueue q;
    // Select short press
    for (uint32_t t = 0; t <= 100; t += 5) buttons.update(true, false, false, t, q);
    for (uint32_t t = 105; t <= 200; t += 5) buttons.update(false, false, false, t, q);
    TEST_ASSERT_EQUAL(Event::Select, q.pop());
    // Back long press
    for (uint32_t t = 1000; t <= 1700; t += 5) buttons.update(false, true, false, t, q);
    TEST_ASSERT_EQUAL(Event::BackLong, q.pop());
    TEST_ASSERT_EQUAL(Event::None, q.pop());
    for (uint32_t t = 1705; t <= 1800; t += 5) buttons.update(false, false, false, t, q);
    // Mode short press
    for (uint32_t t = 2000; t <= 2100; t += 5) buttons.update(false, false, true, t, q);
    for (uint32_t t = 2105; t <= 2200; t += 5) buttons.update(false, false, false, t, q);
    TEST_ASSERT_EQUAL(Event::Mode, q.pop());
    TEST_ASSERT_EQUAL(Event::None, q.pop());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_bounce_shorter_than_debounce_gives_nothing);
    RUN_TEST(test_short_press_fires_on_release);
    RUN_TEST(test_long_press_fires_once_at_the_threshold);
    RUN_TEST(test_no_short_press_after_a_long_press);
    RUN_TEST(test_press_can_repeat_after_release);
    RUN_TEST(test_millis_wraparound_does_not_break_timing);
    RUN_TEST(test_queue_is_first_in_first_out_and_empty_pops_none);
    RUN_TEST(test_queue_overflow_drops_the_oldest);
    RUN_TEST(test_buttons_map_to_events);
    return UNITY_END();
}
