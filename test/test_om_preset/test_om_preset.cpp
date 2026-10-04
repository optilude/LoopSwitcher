#include <unity.h>

#include "rig.h"

void setUp() {}
void tearDown() {}

static void savePresets(Rig& rig) {
    rig.store.savePreset(0, "CLEAN", 0b00000101);
    rig.store.savePreset(1, "CRUNCH", 0b00001010);
    rig.store.savePreset(3, "LEAD", 0b11110000);
    rig.store.savePreset(6, "OFF", 0b00000000);
}

void test_in_preset_mode_footswitch_n_applies_preset_n_and_makes_it_active() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    savePresets(rig);
    rig.controller.toggleMode(0);

    rig.controller.onFootswitch(3, 0);
    TEST_ASSERT_EQUAL_HEX8(0b11110000, rig.loops.stateMask());
    TEST_ASSERT_EQUAL_HEX8(0b11110000, rig.leds.lastMask);
    TEST_ASSERT_EQUAL_UINT8(3, rig.controller.activePreset());

    rig.controller.onFootswitch(1, 0);
    TEST_ASSERT_EQUAL_HEX8(0b00001010, rig.loops.stateMask());
    TEST_ASSERT_EQUAL_UINT8(1, rig.controller.activePreset());
}

void test_a_preset_with_every_loop_off_switches_everything_off() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    savePresets(rig);
    rig.controller.toggleMode(0);
    rig.controller.onFootswitch(3, 0);

    rig.controller.onFootswitch(6, 0);
    TEST_ASSERT_EQUAL_HEX8(0x00, rig.loops.stateMask());
    TEST_ASSERT_EQUAL_HEX8(0x00, rig.leds.lastMask);
    TEST_ASSERT_EQUAL_UINT8(6, rig.controller.activePreset());
}

void test_pressing_the_active_presets_footswitch_again_leaves_its_loops_applied() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    savePresets(rig);
    rig.controller.toggleMode(0);
    rig.controller.onFootswitch(3, 0);
    rig.controller.takeNotice();  // the boot notice

    rig.controller.onFootswitch(3, 0);
    TEST_ASSERT_EQUAL_HEX8(0b11110000, rig.loops.stateMask());
    TEST_ASSERT_EQUAL_UINT8(3, rig.controller.activePreset());
    TEST_ASSERT_EQUAL(Notice::None, rig.controller.takeNotice());
}

void test_an_empty_slot_does_nothing_and_reports_it() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    savePresets(rig);
    rig.controller.toggleMode(0);
    rig.controller.onFootswitch(3, 0);
    rig.controller.takeNotice();
    const int energizeCalls = rig.hw.energizeCalls;

    rig.controller.onFootswitch(2, 0);  // slot 3 is empty
    TEST_ASSERT_EQUAL_INT(energizeCalls, rig.hw.energizeCalls);
    TEST_ASSERT_EQUAL_HEX8(0b11110000, rig.loops.stateMask());
    TEST_ASSERT_EQUAL_UINT8(3, rig.controller.activePreset());
    TEST_ASSERT_EQUAL(Notice::EmptyPreset, rig.controller.takeNotice());
}

void test_entering_preset_mode_applies_the_active_preset() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    savePresets(rig);
    rig.controller.onFootswitch(7, 0);  // manual: loop 8 on

    rig.controller.toggleMode(0);
    TEST_ASSERT_EQUAL(Mode::Preset, rig.controller.mode());
    TEST_ASSERT_EQUAL_UINT8(0, rig.controller.activePreset());
    TEST_ASSERT_EQUAL_HEX8(0b00000101, rig.loops.stateMask());  // preset 1
    TEST_ASSERT_EQUAL_HEX8(0b00000101, rig.leds.lastMask);
}

void test_entering_preset_mode_with_an_empty_active_slot_keeps_the_loops() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();  // no presets saved
    rig.controller.onFootswitch(2, 0);
    rig.controller.takeNotice();

    rig.controller.toggleMode(0);
    TEST_ASSERT_EQUAL(Mode::Preset, rig.controller.mode());
    TEST_ASSERT_EQUAL_HEX8(0b00000100, rig.loops.stateMask());
    TEST_ASSERT_EQUAL(Notice::EmptyPreset, rig.controller.takeNotice());
}

void test_leaving_preset_mode_keeps_the_loops_and_footswitches_toggle_again() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    savePresets(rig);
    rig.controller.toggleMode(0);
    rig.controller.onFootswitch(3, 0);

    rig.controller.toggleMode(0);  // perform
    rig.controller.toggleMode(0);  // manual
    TEST_ASSERT_EQUAL(Mode::Manual, rig.controller.mode());
    TEST_ASSERT_EQUAL_HEX8(0b11110000, rig.loops.stateMask());

    rig.controller.onFootswitch(0, 0);
    TEST_ASSERT_EQUAL_HEX8(0b11110001, rig.loops.stateMask());
    TEST_ASSERT_EQUAL_UINT8(0, rig.controller.lastChangedLoop());
}

void test_leds_show_the_actual_loops_in_both_modes() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    savePresets(rig);

    const uint8_t presses[] = {0, 5, 3, 5, 1};
    for (int round = 0; round < 2; ++round) {
        for (uint8_t press : presses) {
            rig.controller.onFootswitch(press, 0);
            TEST_ASSERT_EQUAL_HEX8(rig.loops.stateMask(), rig.leds.lastMask);
        }
        rig.controller.toggleMode(0);
        TEST_ASSERT_EQUAL_HEX8(rig.loops.stateMask(), rig.leds.lastMask);
    }
}

void test_mode_and_active_preset_reach_the_store() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    savePresets(rig);
    rig.controller.toggleMode(0);
    rig.controller.onFootswitch(3, 0);
    rig.store.flush();

    PresetStore reloaded(eeprom);
    reloaded.begin();
    TEST_ASSERT_TRUE(reloaded.savedState().presetMode);
    TEST_ASSERT_EQUAL_UINT8(3, reloaded.savedState().activePreset);
    TEST_ASSERT_EQUAL_HEX8(0b11110000, reloaded.savedState().loopMask);
}

void test_boot_in_preset_mode_applies_the_active_preset() {
    FakeEeprom eeprom;
    {
        Rig first(eeprom);
        first.boot();
        savePresets(first);
        first.controller.toggleMode(0);
        first.controller.onFootswitch(3, 0);
        first.store.flush();
    }
    Rig second(eeprom);
    TEST_ASSERT_TRUE(second.boot());
    TEST_ASSERT_EQUAL(Mode::Preset, second.controller.mode());
    TEST_ASSERT_EQUAL_UINT8(3, second.controller.activePreset());
    TEST_ASSERT_EQUAL_HEX8(0b11110000, second.loops.stateMask());
    TEST_ASSERT_EQUAL_HEX8(0b11110000, second.leds.lastMask);
}

void test_boot_in_preset_mode_uses_the_preset_even_if_the_saved_loops_differ() {
    FakeEeprom eeprom;
    {
        Rig first(eeprom);
        first.boot();
        savePresets(first);
        first.store.setState(SavedState{0b00110011, true, 1}, 0);  // loops disagree with preset 2
        first.store.flush();
    }
    Rig second(eeprom);
    second.boot();
    TEST_ASSERT_EQUAL_HEX8(0b00001010, second.loops.stateMask());
}

void test_boot_in_preset_mode_with_an_empty_active_slot_uses_the_saved_loops() {
    FakeEeprom eeprom;
    {
        Rig first(eeprom);
        first.boot();
        first.store.setState(SavedState{0b00110011, true, 4}, 0);  // slot 5 is empty
        first.store.flush();
    }
    Rig second(eeprom);
    second.boot();
    TEST_ASSERT_EQUAL(Mode::Preset, second.controller.mode());
    TEST_ASSERT_EQUAL_HEX8(0b00110011, second.loops.stateMask());
}

void test_simultaneous_presses_in_manual_mode_toggle_each_loop_in_order() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();

    rig.controller.onFootswitches(0b00000101, 0);
    TEST_ASSERT_EQUAL_HEX8(0b00000101, rig.loops.stateMask());
    TEST_ASSERT_EQUAL_UINT8(2, rig.controller.lastChangedLoop());  // the highest was handled last
    TEST_ASSERT_EQUAL_HEX8(0b00000101, rig.leds.lastMask);
}

void test_simultaneous_presses_in_preset_mode_apply_only_the_highest() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    savePresets(rig);
    rig.controller.toggleMode(0);

    rig.controller.onFootswitches(0b00001010, 0);  // presets 2 and 4
    TEST_ASSERT_EQUAL_UINT8(3, rig.controller.activePreset());
    TEST_ASSERT_EQUAL_HEX8(0b11110000, rig.loops.stateMask());
}

void test_a_highest_press_on_an_empty_slot_is_reported_and_nothing_else_is_applied() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    savePresets(rig);
    rig.controller.toggleMode(0);
    rig.controller.takeNotice();

    rig.controller.onFootswitches(0b00010001, 0);  // presets 1 and 5 (empty)
    TEST_ASSERT_EQUAL(Notice::EmptyPreset, rig.controller.takeNotice());
    TEST_ASSERT_EQUAL_HEX8(0b00000101, rig.loops.stateMask());  // only the entry into preset mode
}

void test_a_relay_failure_while_selecting_a_preset_is_reported_and_keeps_the_old_preset() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    savePresets(rig);
    rig.controller.toggleMode(0);
    rig.controller.takeNotice();

    rig.hw.failEnergizeOnCall = rig.hw.energizeCalls + 1;
    rig.controller.onFootswitch(3, 0);
    TEST_ASSERT_EQUAL(Notice::Error, rig.controller.takeNotice());
    TEST_ASSERT_EQUAL_UINT8(0, rig.controller.activePreset());
    TEST_ASSERT_EQUAL_HEX8(rig.loops.stateMask(), rig.leds.lastMask);
}

void test_the_leds_follow_a_partially_applied_preset() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    savePresets(rig);
    rig.controller.toggleMode(0);
    rig.controller.takeNotice();
    // Preset 4 turns on loops 5-8: two relay groups. The second group fails.
    rig.hw.failEnergizeOnCall = rig.hw.energizeCalls + 2;
    rig.controller.onFootswitch(3, 0);

    TEST_ASSERT_EQUAL(Notice::Error, rig.controller.takeNotice());
    TEST_ASSERT_EQUAL_HEX8(rig.loops.stateMask(), rig.leds.lastMask);  // LEDs match the relays that moved
}

void test_entering_preset_mode_reports_a_relay_failure_but_still_changes_mode() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    savePresets(rig);
    rig.controller.takeNotice();

    rig.hw.failEnergizeOnCall = rig.hw.energizeCalls + 1;
    rig.controller.toggleMode(0);
    TEST_ASSERT_EQUAL(Mode::Preset, rig.controller.mode());
    TEST_ASSERT_EQUAL(Notice::Error, rig.controller.takeNotice());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_in_preset_mode_footswitch_n_applies_preset_n_and_makes_it_active);
    RUN_TEST(test_a_preset_with_every_loop_off_switches_everything_off);
    RUN_TEST(test_pressing_the_active_presets_footswitch_again_leaves_its_loops_applied);
    RUN_TEST(test_an_empty_slot_does_nothing_and_reports_it);
    RUN_TEST(test_entering_preset_mode_applies_the_active_preset);
    RUN_TEST(test_entering_preset_mode_with_an_empty_active_slot_keeps_the_loops);
    RUN_TEST(test_leaving_preset_mode_keeps_the_loops_and_footswitches_toggle_again);
    RUN_TEST(test_leds_show_the_actual_loops_in_both_modes);
    RUN_TEST(test_mode_and_active_preset_reach_the_store);
    RUN_TEST(test_boot_in_preset_mode_applies_the_active_preset);
    RUN_TEST(test_boot_in_preset_mode_uses_the_preset_even_if_the_saved_loops_differ);
    RUN_TEST(test_boot_in_preset_mode_with_an_empty_active_slot_uses_the_saved_loops);
    RUN_TEST(test_simultaneous_presses_in_manual_mode_toggle_each_loop_in_order);
    RUN_TEST(test_simultaneous_presses_in_preset_mode_apply_only_the_highest);
    RUN_TEST(test_a_highest_press_on_an_empty_slot_is_reported_and_nothing_else_is_applied);
    RUN_TEST(test_a_relay_failure_while_selecting_a_preset_is_reported_and_keeps_the_old_preset);
    RUN_TEST(test_the_leds_follow_a_partially_applied_preset);
    RUN_TEST(test_entering_preset_mode_reports_a_relay_failure_but_still_changes_mode);
    return UNITY_END();
}
