#include <unity.h>

#include "fake_mcp_bus.h"
#include "operating_modes/footswitch_input.h"

void setUp() {}
void tearDown() {}

constexpr uint8_t kU2 = 0x20;

// SWn pulls GPA(8-n) low; loop index n-1.
static uint8_t pin(uint8_t switchNumber) { return static_cast<uint8_t>(1u << (8 - switchNumber)); }
static void press(FakeMcpBus& bus, uint8_t sw) { bus.pinsA = static_cast<uint8_t>(bus.pinsA & ~pin(sw)); }
static void release(FakeMcpBus& bus, uint8_t sw) { bus.pinsA = static_cast<uint8_t>(bus.pinsA | pin(sw)); }

struct Rig {
    Rig() : bus(kU2), expander(bus), input(expander) {
        expander.begin();
        TEST_ASSERT_TRUE(input.begin(0));
    }
    FakeMcpBus bus;
    ControlExpander expander;
    FootswitchInput input;
};

void test_a_press_is_reported_once_at_its_leading_edge() {
    Rig r;
    press(r.bus, 1);
    TEST_ASSERT_EQUAL_HEX8(0b00000001, r.input.poll(true, 100));
    TEST_ASSERT_EQUAL_HEX8(0, r.input.poll(true, 101));  // still held: no repeat
    TEST_ASSERT_EQUAL_HEX8(0, r.input.poll(false, 300));
}

void test_switch_n_reports_loop_n() {
    for (uint8_t sw = 1; sw <= 8; ++sw) {
        Rig r;
        press(r.bus, sw);
        TEST_ASSERT_EQUAL_HEX8(1u << (sw - 1), r.input.poll(true, 100));
    }
}

void test_a_release_is_not_a_press_and_a_new_press_is_reported() {
    Rig r;
    press(r.bus, 3);
    TEST_ASSERT_EQUAL_HEX8(0b00000100, r.input.poll(true, 100));
    release(r.bus, 3);
    TEST_ASSERT_EQUAL_HEX8(0, r.input.poll(true, 200));
    press(r.bus, 3);
    TEST_ASSERT_EQUAL_HEX8(0b00000100, r.input.poll(true, 300));
}

void test_nothing_is_read_without_an_interrupt_until_the_fallback_interval() {
    Rig r;
    press(r.bus, 1);
    const int reads = r.bus.reads;
    TEST_ASSERT_EQUAL_HEX8(0, r.input.poll(false, kFootswitchPollMs - 1));
    TEST_ASSERT_EQUAL_INT(reads, r.bus.reads);
    TEST_ASSERT_EQUAL_HEX8(0b00000001, r.input.poll(false, kFootswitchPollMs));  // found by polling
    TEST_ASSERT_EQUAL_INT(reads + 1, r.bus.reads);
}

void test_an_interrupt_reads_immediately() {
    Rig r;
    press(r.bus, 2);
    const int reads = r.bus.reads;
    TEST_ASSERT_EQUAL_HEX8(0b00000010, r.input.poll(true, 5));
    TEST_ASSERT_EQUAL_INT(reads + 1, r.bus.reads);
}

void test_contact_bounce_inside_the_lockout_gives_one_press() {
    Rig r;
    press(r.bus, 1);
    TEST_ASSERT_EQUAL_HEX8(0b00000001, r.input.poll(true, 1000));
    release(r.bus, 1);
    TEST_ASSERT_EQUAL_HEX8(0, r.input.poll(true, 1010));  // bounce open: ignored
    press(r.bus, 1);
    TEST_ASSERT_EQUAL_HEX8(0, r.input.poll(true, 1015));  // bounce closed: no second press
    TEST_ASSERT_EQUAL_HEX8(0, r.input.poll(false, 1100));
}

void test_a_release_that_happened_inside_the_lockout_is_noticed_afterwards_without_an_interrupt() {
    Rig r;
    press(r.bus, 1);
    r.input.poll(true, 1000);
    release(r.bus, 1);
    r.input.poll(true, 1010);  // ignored, but remembered

    TEST_ASSERT_EQUAL_HEX8(0, r.input.poll(false, 1031));  // the lockout is over: release accepted
    press(r.bus, 1);
    TEST_ASSERT_EQUAL_HEX8(0b00000001, r.input.poll(true, 1200));  // so a real new press works
}

void test_a_press_inside_the_lockout_is_delayed_not_lost() {
    Rig r;
    press(r.bus, 1);
    r.input.poll(true, 1000);
    release(r.bus, 1);
    r.input.poll(true, 1031);  // release accepted, new lockout until 1061
    press(r.bus, 1);
    TEST_ASSERT_EQUAL_HEX8(0, r.input.poll(true, 1040));   // ignored for now
    TEST_ASSERT_EQUAL_HEX8(0b00000001, r.input.poll(false, 1062));  // picked up after the lockout
}

void test_the_lockout_is_per_switch() {
    Rig r;
    press(r.bus, 1);
    TEST_ASSERT_EQUAL_HEX8(0b00000001, r.input.poll(true, 1000));
    press(r.bus, 2);
    TEST_ASSERT_EQUAL_HEX8(0b00000010, r.input.poll(true, 1005));  // SW2 is not locked out by SW1
}

void test_simultaneous_presses_are_reported_together() {
    Rig r;
    press(r.bus, 1);
    press(r.bus, 3);
    press(r.bus, 8);
    TEST_ASSERT_EQUAL_HEX8(0b10000101, r.input.poll(true, 100));
}

void test_a_switch_held_at_boot_is_not_a_press() {
    FakeMcpBus bus(kU2);
    ControlExpander expander(bus);
    expander.begin();
    press(bus, 4);
    FootswitchInput input(expander);
    TEST_ASSERT_TRUE(input.begin(0));

    TEST_ASSERT_EQUAL_HEX8(0, input.poll(true, 10));
    release(bus, 4);
    TEST_ASSERT_EQUAL_HEX8(0, input.poll(true, 500));
    press(bus, 4);
    TEST_ASSERT_EQUAL_HEX8(0b00001000, input.poll(true, 800));
}

void test_a_read_error_reports_nothing_and_the_press_is_found_on_the_next_read() {
    Rig r;
    press(r.bus, 1);
    r.bus.failNext();
    TEST_ASSERT_EQUAL_HEX8(0, r.input.poll(true, 100));
    TEST_ASSERT_EQUAL_UINT32(1, r.input.readFailures());
    TEST_ASSERT_EQUAL_HEX8(0b00000001, r.input.poll(true, 101));
}

void test_a_read_error_is_retried_by_the_fallback_even_without_an_interrupt() {
    Rig r;
    press(r.bus, 1);
    r.bus.failNext();
    r.input.poll(true, 100);
    TEST_ASSERT_EQUAL_HEX8(0, r.input.poll(false, 100 + kFootswitchPollMs - 1));
    TEST_ASSERT_EQUAL_HEX8(0b00000001, r.input.poll(false, 100 + kFootswitchPollMs));
}

void test_begin_reports_a_bus_error() {
    FakeMcpBus bus(kU2);
    ControlExpander expander(bus);
    expander.begin();
    FootswitchInput input(expander);
    bus.failNext();
    TEST_ASSERT_FALSE(input.begin(0));
}

void test_the_lockout_is_30_ms_and_the_fallback_50_ms() {
    TEST_ASSERT_EQUAL_UINT32(30, kFootswitchLockoutMs);
    TEST_ASSERT_EQUAL_UINT32(50, kFootswitchPollMs);
}

void test_timing_survives_millis_wraparound() {
    Rig r;
    const uint32_t t = 0xFFFFFFF0u;
    press(r.bus, 1);
    TEST_ASSERT_EQUAL_HEX8(0b00000001, r.input.poll(true, t));
    release(r.bus, 1);
    r.input.poll(true, t + 5);  // inside the lockout
    press(r.bus, 1);
    TEST_ASSERT_EQUAL_HEX8(0, r.input.poll(true, t + 10));
    release(r.bus, 1);
    r.input.poll(false, t + 70);  // no interrupt: found by the fallback, past the wrap and the lockout
    press(r.bus, 1);
    TEST_ASSERT_EQUAL_HEX8(0b00000001, r.input.poll(true, t + 110));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_a_press_is_reported_once_at_its_leading_edge);
    RUN_TEST(test_switch_n_reports_loop_n);
    RUN_TEST(test_a_release_is_not_a_press_and_a_new_press_is_reported);
    RUN_TEST(test_nothing_is_read_without_an_interrupt_until_the_fallback_interval);
    RUN_TEST(test_an_interrupt_reads_immediately);
    RUN_TEST(test_contact_bounce_inside_the_lockout_gives_one_press);
    RUN_TEST(test_a_release_that_happened_inside_the_lockout_is_noticed_afterwards_without_an_interrupt);
    RUN_TEST(test_a_press_inside_the_lockout_is_delayed_not_lost);
    RUN_TEST(test_the_lockout_is_per_switch);
    RUN_TEST(test_simultaneous_presses_are_reported_together);
    RUN_TEST(test_a_switch_held_at_boot_is_not_a_press);
    RUN_TEST(test_a_read_error_reports_nothing_and_the_press_is_found_on_the_next_read);
    RUN_TEST(test_a_read_error_is_retried_by_the_fallback_even_without_an_interrupt);
    RUN_TEST(test_begin_reports_a_bus_error);
    RUN_TEST(test_the_lockout_is_30_ms_and_the_fallback_50_ms);
    RUN_TEST(test_timing_survives_millis_wraparound);
    return UNITY_END();
}
