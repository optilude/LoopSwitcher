#include <unity.h>

#include "rig.h"

void setUp() {}
void tearDown() {}

void test_boot_on_blank_storage_bypasses_every_loop_and_clears_the_leds() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    TEST_ASSERT_TRUE(rig.boot());

    TEST_ASSERT_EQUAL_HEX8(0x00, rig.loops.stateMask());
    TEST_ASSERT_EQUAL_HEX8(0x00, rig.leds.lastMask);
    TEST_ASSERT_EQUAL(Mode::Manual, rig.controller.mode());
    TEST_ASSERT_EQUAL_UINT8(PerformanceController::kNone, rig.controller.lastChangedLoop());
}

void test_boot_restores_the_saved_loops() {
    FakeEeprom eeprom;
    {
        Rig first(eeprom);
        first.boot();
        first.controller.onFootswitch(1, 0);
        first.controller.onFootswitch(6, 0);
        first.store.flush();
    }
    Rig second(eeprom);
    TEST_ASSERT_TRUE(second.boot());
    TEST_ASSERT_EQUAL_HEX8(0b01000010, second.loops.stateMask());
    TEST_ASSERT_EQUAL_HEX8(0b01000010, second.leds.lastMask);
}

void test_each_footswitch_toggles_its_own_loop() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();

    for (uint8_t i = 0; i < 8; ++i) {
        rig.controller.onFootswitch(i, 0);
        TEST_ASSERT_EQUAL_HEX8(1u << i, rig.loops.stateMask());
        rig.controller.onFootswitch(i, 0);
        TEST_ASSERT_EQUAL_HEX8(0x00, rig.loops.stateMask());
    }
}

void test_leds_always_equal_the_loop_state() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();

    const uint8_t presses[] = {0, 3, 3, 7, 1, 0, 5, 5, 2, 6, 4, 7};
    for (uint8_t press : presses) {
        rig.controller.onFootswitch(press, 0);
        TEST_ASSERT_EQUAL_HEX8(rig.loops.stateMask(), rig.leds.lastMask);
    }
}

void test_the_last_changed_loop_and_its_new_state_are_reported() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();

    rig.controller.onFootswitch(3, 0);
    TEST_ASSERT_EQUAL_UINT8(3, rig.controller.lastChangedLoop());
    TEST_ASSERT_TRUE(rig.controller.lastChangedState());

    rig.controller.onFootswitch(3, 0);
    TEST_ASSERT_EQUAL_UINT8(3, rig.controller.lastChangedLoop());
    TEST_ASSERT_FALSE(rig.controller.lastChangedState());

    rig.controller.onFootswitch(6, 0);
    TEST_ASSERT_EQUAL_UINT8(6, rig.controller.lastChangedLoop());
    TEST_ASSERT_TRUE(rig.controller.lastChangedState());
}

void test_a_failed_change_leaves_leds_state_and_last_change_untouched_and_reports_an_error() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    rig.controller.onFootswitch(2, 0);
    rig.controller.takeNotice();

    rig.hw.failEnergizeOnCall = rig.hw.energizeCalls + 1;
    rig.controller.onFootswitch(5, 0);

    TEST_ASSERT_EQUAL_HEX8(0b00000100, rig.loops.stateMask());
    TEST_ASSERT_EQUAL_HEX8(0b00000100, rig.leds.lastMask);
    TEST_ASSERT_EQUAL_UINT8(2, rig.controller.lastChangedLoop());
    TEST_ASSERT_EQUAL(Notice::Error, rig.controller.takeNotice());
}

void test_notices_are_reported_once() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    rig.hw.failEnergizeOnCall = rig.hw.energizeCalls + 1;
    rig.controller.onFootswitch(0, 0);

    TEST_ASSERT_EQUAL(Notice::Error, rig.controller.takeNotice());
    TEST_ASSERT_EQUAL(Notice::None, rig.controller.takeNotice());
}

void test_an_out_of_range_footswitch_is_ignored() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    const int relayCalls = rig.hw.energizeCalls;
    rig.controller.onFootswitch(8, 0);
    TEST_ASSERT_EQUAL_INT(relayCalls, rig.hw.energizeCalls);
    TEST_ASSERT_EQUAL_HEX8(0x00, rig.loops.stateMask());
}

void test_the_state_reaches_the_store_and_is_saved_after_the_idle_delay() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    rig.controller.onFootswitch(4, 1000);
    rig.controller.tick(1000 + kIdleSaveMs);

    PresetStore reloaded(eeprom);
    reloaded.begin();
    TEST_ASSERT_EQUAL_HEX8(0b00010000, reloaded.savedState().loopMask);
    TEST_ASSERT_FALSE(reloaded.savedState().presetMode);
}

void test_boot_reports_a_data_reset_and_a_relay_failure() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    TEST_ASSERT_TRUE(rig.boot());                                // blank EEPROM: defaults loaded
    TEST_ASSERT_EQUAL(Notice::DataReset, rig.controller.takeNotice());

    Rig second(eeprom);
    second.hw.failEnergizeOnCall = 1;
    TEST_ASSERT_FALSE(second.boot());
    TEST_ASSERT_EQUAL(Notice::Error, second.controller.takeNotice());
}

void test_leds_that_failed_to_update_are_retried_by_tick() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();

    rig.leds.failing = true;
    rig.controller.onFootswitch(1, 1000);
    TEST_ASSERT_EQUAL_HEX8(0x00, rig.leds.lastMask);  // the write failed

    rig.leds.failing = false;
    rig.controller.tick(1050);  // too soon to retry
    TEST_ASSERT_EQUAL_HEX8(0x00, rig.leds.lastMask);
    rig.controller.tick(1000 + kLedRetryMs);
    TEST_ASSERT_EQUAL_HEX8(0b00000010, rig.leds.lastMask);

    const int writes = rig.leds.writes;
    rig.controller.tick(5000);  // nothing left to retry
    TEST_ASSERT_EQUAL_INT(writes, rig.leds.writes);
}

void test_the_loop_mask_is_exposed_for_saving_presets() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    rig.controller.onFootswitch(0, 0);
    rig.controller.onFootswitch(5, 0);
    TEST_ASSERT_EQUAL_HEX8(0b00100001, rig.controller.loopMask());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_the_loop_mask_is_exposed_for_saving_presets);
    RUN_TEST(test_boot_on_blank_storage_bypasses_every_loop_and_clears_the_leds);
    RUN_TEST(test_boot_restores_the_saved_loops);
    RUN_TEST(test_each_footswitch_toggles_its_own_loop);
    RUN_TEST(test_leds_always_equal_the_loop_state);
    RUN_TEST(test_the_last_changed_loop_and_its_new_state_are_reported);
    RUN_TEST(test_a_failed_change_leaves_leds_state_and_last_change_untouched_and_reports_an_error);
    RUN_TEST(test_notices_are_reported_once);
    RUN_TEST(test_an_out_of_range_footswitch_is_ignored);
    RUN_TEST(test_the_state_reaches_the_store_and_is_saved_after_the_idle_delay);
    RUN_TEST(test_boot_reports_a_data_reset_and_a_relay_failure);
    RUN_TEST(test_leds_that_failed_to_update_are_retried_by_tick);
    return UNITY_END();
}
