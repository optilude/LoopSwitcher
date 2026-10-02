#include <unity.h>

#include "fake_display.h"
#include "fake_eeprom.h"
#include "fake_mcp_bus.h"
#include "operating_modes/pedal.h"

void setUp() {}
void tearDown() {}

constexpr uint8_t kU2 = 0x20;
constexpr uint8_t kU3 = 0x21;

// Routes by address to the two expanders on the audio PCB; either can be unplugged.
class Router : public I2cBus {
public:
    Router(FakeMcpBus& u2, FakeMcpBus& u3) : u2_(u2), u3_(u3) {}
    bool u2Present = true;
    bool u3Present = true;

    bool write(uint8_t address, const uint8_t* data, uint8_t length) override {
        if (address == kU2 && u2Present) return u2_.write(address, data, length);
        if (address == kU3 && u3Present) return u3_.write(address, data, length);
        return false;
    }
    bool writeRead(uint8_t address, const uint8_t* out, uint8_t outLength, uint8_t* in, uint8_t inLength) override {
        if (address == kU2 && u2Present) return u2_.writeRead(address, out, outLength, in, inLength);
        if (address == kU3 && u3Present) return u3_.writeRead(address, out, outLength, in, inLength);
        return false;
    }

private:
    FakeMcpBus& u2_;
    FakeMcpBus& u3_;
};

class NoDelay : public Clock {
public:
    void delayMs(uint32_t) override {}
};

struct Bench {
    explicit Bench(FakeEeprom& eeprom) : u2(kU2), u3(kU3), bus(u2, u3), pedal(bus, clock, eeprom) {}

    Pedal::BootStatus boot() { return pedal.begin(now); }
    void update(bool interrupt = true) { pedal.update(interrupt, events, now); }
    void press(uint8_t sw) {
        u2.pinsA = static_cast<uint8_t>(u2.pinsA & ~(1u << (8 - sw)));
        update();
    }
    void release(uint8_t sw) {
        u2.pinsA = static_cast<uint8_t>(u2.pinsA | (1u << (8 - sw)));
        now += 100;
        update();
        now += 100;
    }
    void stomp(uint8_t sw) {  // press and release
        press(sw);
        release(sw);
    }
    void event(Event e) {
        events.push(e);
        now += 5;
        update(false);
    }
    bool toastShows(const char* text) {
        FakeDisplay display;
        pedal.stack().render(display);
        return display.shows(text);
    }
    // The last coil drive written to U3: {OLATA, OLATB}, ignoring the releases.
    bool lastDrive(uint8_t& a, uint8_t& b) const {
        for (size_t i = u3.writes.size(); i-- > 0;) {
            const std::vector<uint8_t>& w = u3.writes[i].bytes;
            if (w.size() == 3 && w[0] == FakeMcpBus::kOlatA && (w[1] != 0 || w[2] != 0)) {
                a = w[1];
                b = w[2];
                return true;
            }
        }
        return false;
    }

    FakeMcpBus u2;
    FakeMcpBus u3;
    Router bus;
    NoDelay clock;
    Pedal pedal;
    EventQueue events;
    uint32_t now = 1000;
};

void test_boot_prepares_both_expanders_and_drives_every_relay_to_bypass() {
    FakeEeprom eeprom;
    Bench b(eeprom);
    const Pedal::BootStatus status = b.boot();

    TEST_ASSERT_TRUE(status.relays);
    TEST_ASSERT_TRUE(status.controls);
    TEST_ASSERT_FALSE(status.storageValid);  // blank EEPROM

    TEST_ASSERT_EQUAL_HEX8(0x00, b.u3.reg(FakeMcpBus::kIodirA));  // relay pins are outputs
    TEST_ASSERT_EQUAL_HEX8(0x00, b.u3.reg(FakeMcpBus::kOlatA));   // and nothing is left energized
    TEST_ASSERT_EQUAL_HEX8(0x00, b.u3.reg(FakeMcpBus::kOlatB));
    TEST_ASSERT_EQUAL_HEX8(0xFF, b.u2.reg(FakeMcpBus::kIodirA));  // footswitches are inputs
    TEST_ASSERT_EQUAL_HEX8(0x00, b.u2.reg(FakeMcpBus::kOlatB));   // all LEDs off

    // The RST coils of all eight relays were pulsed (loops 1-4 on port A, 5-8 on port B).
    bool a = false, bport = false;
    for (const Transfer& t : b.u3.writes) {
        if (t.bytes.size() == 3 && t.bytes[0] == FakeMcpBus::kOlatA) {
            if (t.bytes[1] == 0x55 && t.bytes[2] == 0x00) a = true;
            if (t.bytes[1] == 0x00 && t.bytes[2] == 0x55) bport = true;
        }
    }
    TEST_ASSERT_TRUE(a);
    TEST_ASSERT_TRUE(bport);
}

void test_boot_shows_the_play_screen_and_a_data_reset_notice() {
    FakeEeprom eeprom;
    Bench b(eeprom);
    b.boot();
    TEST_ASSERT_EQUAL_UINT8(1, b.pedal.stack().depth());
    TEST_ASSERT_TRUE(b.toastShows("DATA RESET"));
    TEST_ASSERT_TRUE(b.toastShows("MANUAL"));
}

void test_a_footswitch_press_pulses_the_right_coil_and_lights_the_right_led() {
    FakeEeprom eeprom;
    Bench b(eeprom);
    b.boot();

    b.press(1);  // SW1: loop 1
    uint8_t a = 0, bp = 0;
    TEST_ASSERT_TRUE(b.lastDrive(a, bp));
    TEST_ASSERT_EQUAL_HEX8(0b00000010, a);  // loop 1 SET = GPA1
    TEST_ASSERT_EQUAL_HEX8(0x00, bp);
    TEST_ASSERT_EQUAL_HEX8(0x00, b.u3.reg(FakeMcpBus::kOlatA));  // the coil was released again
    TEST_ASSERT_EQUAL_HEX8(0b10000000, b.u2.reg(FakeMcpBus::kOlatB));  // LED1 = GPB7
}

void test_footswitch_eight_uses_port_b_and_led_eight() {
    FakeEeprom eeprom;
    Bench b(eeprom);
    b.boot();
    b.press(8);

    uint8_t a = 0, bp = 0;
    TEST_ASSERT_TRUE(b.lastDrive(a, bp));
    TEST_ASSERT_EQUAL_HEX8(0x00, a);
    TEST_ASSERT_EQUAL_HEX8(0b10000000, bp);  // loop 8 SET = GPB7
    TEST_ASSERT_EQUAL_HEX8(0b00000001, b.u2.reg(FakeMcpBus::kOlatB));  // LED8 = GPB0
}

void test_pressing_again_resets_the_loop_and_turns_the_led_off() {
    FakeEeprom eeprom;
    Bench b(eeprom);
    b.boot();
    b.stomp(3);
    b.stomp(3);

    uint8_t a = 0, bp = 0;
    TEST_ASSERT_TRUE(b.lastDrive(a, bp));
    TEST_ASSERT_EQUAL_HEX8(0b00010000, a);  // loop 3 RST = GPA4
    TEST_ASSERT_EQUAL_HEX8(0x00, b.u2.reg(FakeMcpBus::kOlatB));
}

void test_holding_a_footswitch_acts_once() {
    FakeEeprom eeprom;
    Bench b(eeprom);
    b.boot();
    b.press(2);
    const size_t writes = b.u3.writes.size();
    for (int i = 0; i < 5; ++i) {
        b.now += 7;
        b.update();
    }
    TEST_ASSERT_EQUAL_UINT(writes, b.u3.writes.size());
}

void test_the_mode_button_toggles_modes_but_is_ignored_inside_menus() {
    FakeEeprom eeprom;
    Bench b(eeprom);
    b.boot();
    b.event(Event::Mode);
    TEST_ASSERT_EQUAL(Mode::Preset, b.pedal.controller().mode());

    b.event(Event::Mode);
    TEST_ASSERT_EQUAL(Mode::Manual, b.pedal.controller().mode());

    b.event(Event::Select);  // opens the menu
    TEST_ASSERT_EQUAL_UINT8(2, b.pedal.stack().depth());
    b.event(Event::Mode);
    TEST_ASSERT_EQUAL(Mode::Manual, b.pedal.controller().mode());
}

void test_footswitches_keep_working_while_a_menu_is_open() {
    FakeEeprom eeprom;
    Bench b(eeprom);
    b.boot();
    b.event(Event::Select);
    TEST_ASSERT_EQUAL_UINT8(2, b.pedal.stack().depth());

    b.stomp(2);
    TEST_ASSERT_EQUAL_HEX8(0b00000010, b.pedal.controller().loopMask());
    TEST_ASSERT_EQUAL_HEX8(0b01000000, b.u2.reg(FakeMcpBus::kOlatB));  // LED2 = GPB6
    TEST_ASSERT_EQUAL_UINT8(2, b.pedal.stack().depth());                // the menu stayed open
}

void test_a_footswitch_press_keeps_a_menu_from_timing_out() {
    FakeEeprom eeprom;
    Bench b(eeprom);
    b.boot();
    b.event(Event::Select);
    b.now += kMenuIdleMs - 1000;
    b.stomp(1);  // 200 ms later
    b.now += kMenuIdleMs - 1000;
    b.update(false);
    TEST_ASSERT_EQUAL_UINT8(2, b.pedal.stack().depth());

    b.now += 2000;
    b.update(false);
    TEST_ASSERT_EQUAL_UINT8(1, b.pedal.stack().depth());
}

void test_preset_mode_applies_the_saved_loops_to_the_relays_and_leds() {
    FakeEeprom eeprom;
    Bench b(eeprom);
    b.boot();
    b.pedal.store().savePreset(1, "LEAD", 0b00001001);  // loops 1 and 4

    b.event(Event::Mode);  // preset mode; the active preset (1) is empty
    b.stomp(2);            // preset 2
    TEST_ASSERT_EQUAL_HEX8(0b00001001, b.pedal.controller().loopMask());
    TEST_ASSERT_EQUAL_HEX8(0b10010000, b.u2.reg(FakeMcpBus::kOlatB));  // LED1 and LED4
    uint8_t a = 0, bp = 0;
    TEST_ASSERT_TRUE(b.lastDrive(a, bp));
    TEST_ASSERT_EQUAL_HEX8(0b10000010, a);  // loop 1 SET = GPA1, loop 4 SET = GPA7
    TEST_ASSERT_TRUE(b.toastShows("LEAD"));
}

void test_an_empty_preset_slot_shows_a_notice_and_moves_nothing() {
    FakeEeprom eeprom;
    Bench b(eeprom);
    b.boot();
    b.event(Event::Mode);
    const size_t writes = b.u3.writes.size();
    b.stomp(5);
    TEST_ASSERT_EQUAL_UINT(writes, b.u3.writes.size());
    TEST_ASSERT_TRUE(b.toastShows("EMPTY"));
}

void test_simultaneous_presses_in_preset_mode_use_the_highest() {
    FakeEeprom eeprom;
    Bench b(eeprom);
    b.boot();
    b.pedal.store().savePreset(0, "ONE", 0b00000001);
    b.pedal.store().savePreset(3, "FOUR", 0b00001000);
    b.event(Event::Mode);

    b.u2.pinsA = static_cast<uint8_t>(b.u2.pinsA & ~((1u << 7) | (1u << 4)));  // SW1 and SW4 together
    b.update();
    TEST_ASSERT_EQUAL_UINT8(3, b.pedal.controller().activePreset());
    TEST_ASSERT_EQUAL_HEX8(0b00001000, b.pedal.controller().loopMask());
}

void test_the_state_survives_a_reboot_and_is_driven_back_onto_the_relays() {
    FakeEeprom eeprom;
    {
        Bench first(eeprom);
        first.boot();
        first.stomp(1);
        first.stomp(6);
        first.pedal.store().flush();
    }
    Bench second(eeprom);
    const Pedal::BootStatus status = second.boot();
    TEST_ASSERT_TRUE(status.storageValid);
    TEST_ASSERT_EQUAL_HEX8(0b00100001, second.pedal.controller().loopMask());
    TEST_ASSERT_EQUAL_HEX8(0b10000100, second.u2.reg(FakeMcpBus::kOlatB));  // LED1 and LED6
    TEST_ASSERT_FALSE(second.toastShows("DATA RESET"));

    // Boot re-pulses the SET coils of loops 1 and 6 and the RST coils of the others.
    uint8_t a = 0, bp = 0;
    TEST_ASSERT_TRUE(second.lastDrive(a, bp));
}

void test_a_missing_relay_board_is_reported_and_leaves_the_leds_alone() {
    FakeEeprom eeprom;
    Bench b(eeprom);
    b.bus.u3Present = false;
    const Pedal::BootStatus status = b.boot();
    TEST_ASSERT_FALSE(status.relays);
    TEST_ASSERT_TRUE(status.controls);
    TEST_ASSERT_TRUE(b.toastShows("ERROR"));

    b.stomp(1);
    TEST_ASSERT_EQUAL_HEX8(0x00, b.pedal.controller().loopMask());
    TEST_ASSERT_EQUAL_HEX8(0x00, b.u2.reg(FakeMcpBus::kOlatB));  // LEDs never claim a loop is on
    TEST_ASSERT_TRUE(b.toastShows("ERROR"));
}

void test_a_missing_control_board_is_reported_but_the_menus_still_work() {
    FakeEeprom eeprom;
    Bench b(eeprom);
    b.bus.u2Present = false;
    const Pedal::BootStatus status = b.boot();
    TEST_ASSERT_TRUE(status.relays);
    TEST_ASSERT_FALSE(status.controls);
    TEST_ASSERT_TRUE(b.toastShows("ERROR"));

    b.event(Event::Select);
    TEST_ASSERT_EQUAL_UINT8(2, b.pedal.stack().depth());
}

void test_a_relay_failure_during_play_is_reported_and_play_continues() {
    FakeEeprom eeprom;
    Bench b(eeprom);
    b.boot();
    b.bus.u3Present = false;
    b.stomp(1);
    TEST_ASSERT_TRUE(b.toastShows("ERROR"));

    b.bus.u3Present = true;
    b.now += 5000;
    b.stomp(1);
    TEST_ASSERT_EQUAL_HEX8(0b00000001, b.pedal.controller().loopMask());
}

void test_update_without_any_activity_does_not_touch_the_relays_or_the_leds() {
    FakeEeprom eeprom;
    Bench b(eeprom);
    b.boot();
    const size_t u3 = b.u3.writes.size();
    const size_t u2 = b.u2.writes.size();
    for (int i = 0; i < 10; ++i) {
        b.now += 3;
        b.update(false);
    }
    TEST_ASSERT_EQUAL_UINT(u3, b.u3.writes.size());
    TEST_ASSERT_EQUAL_UINT(u2, b.u2.writes.size());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_boot_prepares_both_expanders_and_drives_every_relay_to_bypass);
    RUN_TEST(test_boot_shows_the_play_screen_and_a_data_reset_notice);
    RUN_TEST(test_a_footswitch_press_pulses_the_right_coil_and_lights_the_right_led);
    RUN_TEST(test_footswitch_eight_uses_port_b_and_led_eight);
    RUN_TEST(test_pressing_again_resets_the_loop_and_turns_the_led_off);
    RUN_TEST(test_holding_a_footswitch_acts_once);
    RUN_TEST(test_the_mode_button_toggles_modes_but_is_ignored_inside_menus);
    RUN_TEST(test_footswitches_keep_working_while_a_menu_is_open);
    RUN_TEST(test_a_footswitch_press_keeps_a_menu_from_timing_out);
    RUN_TEST(test_preset_mode_applies_the_saved_loops_to_the_relays_and_leds);
    RUN_TEST(test_an_empty_preset_slot_shows_a_notice_and_moves_nothing);
    RUN_TEST(test_simultaneous_presses_in_preset_mode_use_the_highest);
    RUN_TEST(test_the_state_survives_a_reboot_and_is_driven_back_onto_the_relays);
    RUN_TEST(test_a_missing_relay_board_is_reported_and_leaves_the_leds_alone);
    RUN_TEST(test_a_missing_control_board_is_reported_but_the_menus_still_work);
    RUN_TEST(test_a_relay_failure_during_play_is_reported_and_play_continues);
    RUN_TEST(test_update_without_any_activity_does_not_touch_the_relays_or_the_leds);
    return UNITY_END();
}
