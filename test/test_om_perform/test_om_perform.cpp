#include <unity.h>

#include "fake_display.h"
#include "operating_modes/play_screen.h"
#include "rig.h"

void setUp() {}
void tearDown() {}

static void savePresets(Rig& rig) {
    rig.store.savePreset(0, "CLEAN", 0b00000101);
    rig.store.savePreset(3, "LEAD", 0b11110000);
}

static void enterPerform(Rig& rig) {
    rig.controller.toggleMode(0);  // preset
    rig.controller.toggleMode(0);  // perform
    rig.controller.takeNotice();
}

// A stomp of `heldMs` on footswitch `index`, starting at `startMs`.
static void stomp(Rig& rig, uint8_t index, uint32_t startMs, uint32_t heldMs) {
    rig.controller.onFootswitches(static_cast<uint8_t>(1u << index), startMs);
    rig.controller.tick(startMs + heldMs);
    rig.controller.onFootswitchesReleased(static_cast<uint8_t>(1u << index), startMs + heldMs);
}

void test_the_mode_button_cycles_manual_preset_perform() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    TEST_ASSERT_EQUAL(Mode::Manual, rig.controller.mode());
    rig.controller.toggleMode(0);
    TEST_ASSERT_EQUAL(Mode::Preset, rig.controller.mode());
    rig.controller.toggleMode(0);
    TEST_ASSERT_EQUAL(Mode::Perform, rig.controller.mode());
    rig.controller.toggleMode(0);
    TEST_ASSERT_EQUAL(Mode::Manual, rig.controller.mode());
}

void test_entering_perform_mode_applies_the_active_preset() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    savePresets(rig);
    rig.controller.toggleMode(0);
    rig.controller.onFootswitch(3, 0);  // preset 4
    rig.controller.onFootswitch(0, 0);  // back to preset 1
    rig.controller.toggleMode(0);
    TEST_ASSERT_EQUAL(Mode::Perform, rig.controller.mode());
    TEST_ASSERT_EQUAL_HEX8(0b00000101, rig.loops.stateMask());
}

void test_a_press_does_nothing_until_it_is_released() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    enterPerform(rig);

    rig.controller.onFootswitches(0b00000100, 1000);
    rig.controller.tick(1500);
    TEST_ASSERT_EQUAL_HEX8(0x00, rig.loops.stateMask());
}

void test_a_short_press_toggles_the_loop() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    enterPerform(rig);

    stomp(rig, 2, 1000, 150);
    TEST_ASSERT_EQUAL_HEX8(0b00000100, rig.loops.stateMask());
    TEST_ASSERT_EQUAL_HEX8(0b00000100, rig.leds.lastMask);
    TEST_ASSERT_EQUAL_UINT8(2, rig.controller.lastChangedLoop());
    TEST_ASSERT_TRUE(rig.controller.lastChangedState());

    stomp(rig, 2, 2000, 150);
    TEST_ASSERT_EQUAL_HEX8(0x00, rig.loops.stateMask());
    TEST_ASSERT_FALSE(rig.controller.lastChangedState());
}

void test_a_short_press_does_not_change_the_active_preset() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    savePresets(rig);
    enterPerform(rig);

    stomp(rig, 3, 1000, 150);  // slot 4 is used, but this is a short press
    TEST_ASSERT_EQUAL_UINT8(0, rig.controller.activePreset());
    TEST_ASSERT_EQUAL_HEX8(0b00000101 ^ 0b00001000, rig.loops.stateMask());
}

void test_a_long_press_selects_the_preset_while_the_switch_is_still_held() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    savePresets(rig);
    enterPerform(rig);

    rig.controller.onFootswitches(0b00001000, 1000);
    TEST_ASSERT_FALSE(rig.controller.tick(1000 + kPerformHoldMs - 1));
    TEST_ASSERT_EQUAL_UINT8(0, rig.controller.activePreset());

    TEST_ASSERT_TRUE(rig.controller.tick(1000 + kPerformHoldMs));
    TEST_ASSERT_EQUAL_UINT8(3, rig.controller.activePreset());
    TEST_ASSERT_EQUAL_HEX8(0b11110000, rig.loops.stateMask());
    TEST_ASSERT_EQUAL_HEX8(0b11110000, rig.leds.lastMask);
}

void test_releasing_after_a_long_press_does_not_toggle_the_loop() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    savePresets(rig);
    enterPerform(rig);

    rig.controller.onFootswitches(0b00001000, 1000);
    rig.controller.tick(1000 + kPerformHoldMs);
    rig.controller.onFootswitchesReleased(0b00001000, 6000);
    TEST_ASSERT_EQUAL_HEX8(0b11110000, rig.loops.stateMask());
}

void test_a_release_after_the_threshold_but_before_a_tick_still_counts_as_long() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    savePresets(rig);
    enterPerform(rig);

    stomp(rig, 3, 1000, kPerformHoldMs + 10);
    TEST_ASSERT_EQUAL_UINT8(3, rig.controller.activePreset());
    TEST_ASSERT_EQUAL_HEX8(0b11110000, rig.loops.stateMask());
}

void test_a_long_press_on_an_empty_slot_reports_it_and_changes_nothing() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    savePresets(rig);
    enterPerform(rig);

    stomp(rig, 5, 1000, kPerformHoldMs);
    TEST_ASSERT_EQUAL(Notice::EmptyPreset, rig.controller.takeNotice());
    TEST_ASSERT_EQUAL_HEX8(0b00000101, rig.loops.stateMask());
}

void test_stomping_loops_after_a_preset_keeps_the_preset_active() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    savePresets(rig);
    enterPerform(rig);

    stomp(rig, 3, 1000, kPerformHoldMs);
    stomp(rig, 4, 6000, 100);
    TEST_ASSERT_EQUAL_UINT8(3, rig.controller.activePreset());
    TEST_ASSERT_EQUAL_HEX8(0b11110000 ^ 0b00010000, rig.loops.stateMask());
}

void test_two_long_presses_together_select_only_the_highest() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    savePresets(rig);
    enterPerform(rig);

    rig.controller.onFootswitches(0b00001001, 1000);
    rig.controller.tick(1000 + kPerformHoldMs);
    TEST_ASSERT_EQUAL_UINT8(3, rig.controller.activePreset());
    rig.controller.onFootswitchesReleased(0b00001001, 5000);
    TEST_ASSERT_EQUAL_HEX8(0b11110000, rig.loops.stateMask());
}

void test_simultaneous_short_presses_toggle_each_loop() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    enterPerform(rig);

    rig.controller.onFootswitches(0b00000101, 1000);
    rig.controller.onFootswitchesReleased(0b00000101, 1100);
    TEST_ASSERT_EQUAL_HEX8(0b00000101, rig.loops.stateMask());
}

void test_changing_mode_while_a_switch_is_held_drops_the_press() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    savePresets(rig);
    enterPerform(rig);

    rig.controller.onFootswitches(0b00001000, 1000);
    rig.controller.toggleMode(1500);  // back to manual
    TEST_ASSERT_FALSE(rig.controller.tick(1000 + kPerformHoldMs));
    rig.controller.onFootswitchesReleased(0b00001000, 4500);
    TEST_ASSERT_EQUAL(Mode::Manual, rig.controller.mode());
    TEST_ASSERT_EQUAL_HEX8(0b00000101, rig.loops.stateMask());
}

void test_releases_outside_perform_mode_are_ignored() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();

    rig.controller.onFootswitchesReleased(0b00000001, 100);
    TEST_ASSERT_EQUAL_HEX8(0x00, rig.loops.stateMask());
}

void test_perform_mode_and_loops_survive_a_power_cycle() {
    FakeEeprom eeprom;
    {
        Rig first(eeprom);
        first.boot();
        savePresets(first);
        enterPerform(first);
        stomp(first, 3, 1000, kPerformHoldMs);
        stomp(first, 4, 6000, 100);
        first.store.flush();
    }
    Rig second(eeprom);
    TEST_ASSERT_TRUE(second.boot());
    TEST_ASSERT_EQUAL(Mode::Perform, second.controller.mode());
    TEST_ASSERT_EQUAL_UINT8(3, second.controller.activePreset());
    TEST_ASSERT_EQUAL_HEX8(0b11110000 ^ 0b00010000, second.loops.stateMask());  // not the preset's own mask
}

void test_the_screen_shows_the_perform_title_preset_name_and_last_stomp() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    savePresets(rig);
    rig.store.setLoopLabel(4, "DELAY");
    enterPerform(rig);
    PlayScreen screen(rig.controller, rig.store, nullptr, nullptr);
    FakeDisplay display;

    screen.draw(display);
    TEST_ASSERT_TRUE(display.shows("PERFORM 1"));
    TEST_ASSERT_TRUE(display.shows("CLEAN"));
    TEST_ASSERT_FALSE(display.shows("DELAY"));

    stomp(rig, 4, 1000, 100);
    display.drawn.clear();
    screen.draw(display);
    TEST_ASSERT_TRUE(display.shows("PERFORM 1"));
    TEST_ASSERT_TRUE(display.shows("CLEAN"));
    TEST_ASSERT_TRUE(display.shows("DELAY ON"));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_the_mode_button_cycles_manual_preset_perform);
    RUN_TEST(test_entering_perform_mode_applies_the_active_preset);
    RUN_TEST(test_a_press_does_nothing_until_it_is_released);
    RUN_TEST(test_a_short_press_toggles_the_loop);
    RUN_TEST(test_a_short_press_does_not_change_the_active_preset);
    RUN_TEST(test_a_long_press_selects_the_preset_while_the_switch_is_still_held);
    RUN_TEST(test_releasing_after_a_long_press_does_not_toggle_the_loop);
    RUN_TEST(test_a_release_after_the_threshold_but_before_a_tick_still_counts_as_long);
    RUN_TEST(test_a_long_press_on_an_empty_slot_reports_it_and_changes_nothing);
    RUN_TEST(test_stomping_loops_after_a_preset_keeps_the_preset_active);
    RUN_TEST(test_two_long_presses_together_select_only_the_highest);
    RUN_TEST(test_simultaneous_short_presses_toggle_each_loop);
    RUN_TEST(test_changing_mode_while_a_switch_is_held_drops_the_press);
    RUN_TEST(test_releases_outside_perform_mode_are_ignored);
    RUN_TEST(test_perform_mode_and_loops_survive_a_power_cycle);
    RUN_TEST(test_the_screen_shows_the_perform_title_preset_name_and_last_stomp);
    return UNITY_END();
}
