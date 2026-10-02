#include <unity.h>

#include "fake_mcp_bus.h"
#include "operating_modes/control_expander.h"

void setUp() {}
void tearDown() {}

constexpr uint8_t kU2 = 0x20;

void test_begin_makes_the_footswitches_inputs_with_pull_ups_and_the_leds_outputs() {
    FakeMcpBus bus(kU2);
    ControlExpander expander(bus);
    TEST_ASSERT_TRUE(expander.begin());

    TEST_ASSERT_EQUAL_HEX8(0xFF, bus.reg(FakeMcpBus::kIodirA));  // port A: footswitches in
    TEST_ASSERT_EQUAL_HEX8(0x00, bus.reg(FakeMcpBus::kIodirB));  // port B: LEDs out
    TEST_ASSERT_EQUAL_HEX8(0xFF, bus.reg(FakeMcpBus::kGppuA));   // pull-ups on the footswitches
    TEST_ASSERT_EQUAL_HEX8(0xFF, bus.reg(FakeMcpBus::kIpolA));   // a pressed switch reads 1
    TEST_ASSERT_EQUAL_HEX8(0xFF, bus.reg(FakeMcpBus::kGpintenA));
    TEST_ASSERT_EQUAL_HEX8(0x00, bus.reg(FakeMcpBus::kIntconA));  // interrupt on any change
    TEST_ASSERT_EQUAL_HEX8(0x00, bus.reg(FakeMcpBus::kOlatB));    // LEDs off
}

void test_begin_switches_the_leds_off_before_the_pins_become_outputs() {
    FakeMcpBus bus(kU2);
    ControlExpander expander(bus);
    expander.begin();

    TEST_ASSERT_TRUE(bus.writes.size() >= 2);
    TEST_ASSERT_EQUAL_HEX8(FakeMcpBus::kOlatB, bus.writes[0].bytes[0]);  // the latch first ...
    TEST_ASSERT_EQUAL_HEX8(0x00, bus.writes[0].bytes[1]);
    TEST_ASSERT_EQUAL_HEX8(FakeMcpBus::kIodirA, bus.writes[1].bytes[0]);  // ... then the directions
}

void test_every_transfer_goes_to_the_control_expanders_address() {
    FakeMcpBus bus(kU2);
    ControlExpander expander(bus);
    expander.begin();
    expander.setLeds(0x55);
    for (const Transfer& t : bus.writes) TEST_ASSERT_EQUAL_HEX8(kU2, t.address);
}

void test_begin_reports_a_bus_failure_at_any_step_and_stops_there() {
    FakeMcpBus probe(kU2);
    ControlExpander(probe).begin();
    const int steps = probe.calls();
    TEST_ASSERT_TRUE(steps >= 6);

    for (int failing = 1; failing <= steps; ++failing) {
        FakeMcpBus bus(kU2);
        bus.failOnCall = failing;
        ControlExpander expander(bus);
        TEST_ASSERT_FALSE_MESSAGE(expander.begin(), "begin() must report the failure");
        TEST_ASSERT_EQUAL_INT(failing, bus.calls());  // and stop at the failing step
    }
}

void test_loop_n_lights_led_n_on_port_b_in_reverse_order() {
    // LED1 = GPB7 ... LED8 = GPB0
    for (uint8_t loop = 0; loop < 8; ++loop) {
        FakeMcpBus bus(kU2);
        ControlExpander expander(bus);
        expander.begin();
        TEST_ASSERT_TRUE(expander.setLeds(static_cast<uint8_t>(1u << loop)));
        TEST_ASSERT_EQUAL_HEX8(1u << (7 - loop), bus.reg(FakeMcpBus::kOlatB));
    }
}

void test_all_leds_and_no_leds() {
    FakeMcpBus bus(kU2);
    ControlExpander expander(bus);
    expander.begin();
    expander.setLeds(0xFF);
    TEST_ASSERT_EQUAL_HEX8(0xFF, bus.reg(FakeMcpBus::kOlatB));
    expander.setLeds(0x00);
    TEST_ASSERT_EQUAL_HEX8(0x00, bus.reg(FakeMcpBus::kOlatB));
}

void test_a_mixed_led_pattern() {
    FakeMcpBus bus(kU2);
    ControlExpander expander(bus);
    expander.begin();
    expander.setLeds(0b00000101);  // LED1 and LED3 = GPB7 and GPB5
    TEST_ASSERT_EQUAL_HEX8(0b10100000, bus.reg(FakeMcpBus::kOlatB));
}

void test_setting_the_leds_never_touches_the_footswitch_port() {
    FakeMcpBus bus(kU2);
    ControlExpander expander(bus);
    expander.begin();
    const uint8_t gppu = bus.reg(FakeMcpBus::kGppuA);
    expander.setLeds(0xA5);
    TEST_ASSERT_EQUAL_HEX8(gppu, bus.reg(FakeMcpBus::kGppuA));
    TEST_ASSERT_EQUAL_HEX8(0xFF, bus.reg(FakeMcpBus::kIodirA));
    TEST_ASSERT_EQUAL_HEX8(0x00, bus.reg(FakeMcpBus::kOlatA));
}

void test_a_led_write_failure_is_reported() {
    FakeMcpBus bus(kU2);
    ControlExpander expander(bus);
    expander.begin();
    bus.failNext();
    TEST_ASSERT_FALSE(expander.setLeds(0x01));
}

void test_with_nothing_pressed_the_mask_is_empty() {
    FakeMcpBus bus(kU2);
    ControlExpander expander(bus);
    expander.begin();
    uint8_t pressed = 0xAA;
    TEST_ASSERT_TRUE(expander.readSwitches(pressed));
    TEST_ASSERT_EQUAL_HEX8(0x00, pressed);
}

void test_footswitch_n_is_loop_n_and_sw1_is_gpa7() {
    // SW1 = GPA7 ... SW8 = GPA0; a pressed switch pulls its pin low.
    for (uint8_t n = 1; n <= 8; ++n) {
        FakeMcpBus bus(kU2);
        ControlExpander expander(bus);
        expander.begin();
        bus.pinsA = static_cast<uint8_t>(~(1u << (8 - n)));

        uint8_t pressed = 0;
        TEST_ASSERT_TRUE(expander.readSwitches(pressed));
        TEST_ASSERT_EQUAL_HEX8(1u << (n - 1), pressed);
    }
}

void test_several_footswitches_pressed_together() {
    FakeMcpBus bus(kU2);
    ControlExpander expander(bus);
    expander.begin();
    bus.pinsA = static_cast<uint8_t>(~((1u << 7) | (1u << 5) | (1u << 0)));  // SW1, SW3, SW8

    uint8_t pressed = 0;
    expander.readSwitches(pressed);
    TEST_ASSERT_EQUAL_HEX8(0b10000101, pressed);
}

void test_the_led_port_does_not_leak_into_the_switch_reading() {
    FakeMcpBus bus(kU2);
    ControlExpander expander(bus);
    expander.begin();
    expander.setLeds(0xFF);
    uint8_t pressed = 0xFF;
    expander.readSwitches(pressed);
    TEST_ASSERT_EQUAL_HEX8(0x00, pressed);
}

void test_a_read_failure_is_reported_and_leaves_the_mask_alone() {
    FakeMcpBus bus(kU2);
    ControlExpander expander(bus);
    expander.begin();
    bus.failNext();
    uint8_t pressed = 0x42;
    TEST_ASSERT_FALSE(expander.readSwitches(pressed));
    TEST_ASSERT_EQUAL_HEX8(0x42, pressed);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_begin_makes_the_footswitches_inputs_with_pull_ups_and_the_leds_outputs);
    RUN_TEST(test_begin_switches_the_leds_off_before_the_pins_become_outputs);
    RUN_TEST(test_every_transfer_goes_to_the_control_expanders_address);
    RUN_TEST(test_begin_reports_a_bus_failure_at_any_step_and_stops_there);
    RUN_TEST(test_loop_n_lights_led_n_on_port_b_in_reverse_order);
    RUN_TEST(test_all_leds_and_no_leds);
    RUN_TEST(test_a_mixed_led_pattern);
    RUN_TEST(test_setting_the_leds_never_touches_the_footswitch_port);
    RUN_TEST(test_a_led_write_failure_is_reported);
    RUN_TEST(test_with_nothing_pressed_the_mask_is_empty);
    RUN_TEST(test_footswitch_n_is_loop_n_and_sw1_is_gpa7);
    RUN_TEST(test_several_footswitches_pressed_together);
    RUN_TEST(test_the_led_port_does_not_leak_into_the_switch_reading);
    RUN_TEST(test_a_read_failure_is_reported_and_leaves_the_mask_alone);
    return UNITY_END();
}
