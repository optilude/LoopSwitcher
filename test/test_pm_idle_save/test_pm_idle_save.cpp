#include <unity.h>

#include "fake_eeprom.h"
#include "preset_management/layout.h"
#include "preset_management/preset_store.h"

void setUp() {}
void tearDown() {}

static SavedState state(uint8_t mask, bool presetMode = false, uint8_t preset = 0) {
    return SavedState{mask, presetMode, preset};
}

static void assertState(const SavedState& expected, const SavedState& actual) {
    TEST_ASSERT_EQUAL_HEX8(expected.loopMask, actual.loopMask);
    TEST_ASSERT_EQUAL(expected.presetMode, actual.presetMode);
    TEST_ASSERT_EQUAL_UINT8(expected.activePreset, actual.activePreset);
}

void test_nothing_is_written_before_the_idle_delay() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    eeprom.resetWriteCounts();

    store.setState(state(0x05), 1000);
    store.tick(1000);
    store.tick(2999);
    TEST_ASSERT_EQUAL_UINT32(0, eeprom.totalWrites());
}

void test_the_state_is_written_once_the_idle_delay_has_passed() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    store.setState(state(0x05, true, 2), 1000);

    store.tick(1000 + kIdleSaveMs);
    PresetStore reloaded(eeprom);
    reloaded.begin();
    assertState(state(0x05, true, 2), reloaded.savedState());
}

void test_it_is_written_exactly_once() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    store.setState(state(0x05), 0);
    store.tick(kIdleSaveMs);
    const uint32_t afterFirst = eeprom.totalWrites();

    store.tick(kIdleSaveMs + 10);
    store.tick(kIdleSaveMs * 5);
    TEST_ASSERT_EQUAL_UINT32(afterFirst, eeprom.totalWrites());
}

void test_a_further_change_restarts_the_delay_and_only_the_last_state_is_saved() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    eeprom.resetWriteCounts();

    store.setState(state(0x01), 0);
    store.setState(state(0x02), 1500);
    store.tick(2500);  // only 1000 ms since the last change
    TEST_ASSERT_EQUAL_UINT32(0, eeprom.totalWrites());

    store.tick(1500 + kIdleSaveMs);
    PresetStore reloaded(eeprom);
    reloaded.begin();
    assertState(state(0x02), reloaded.savedState());
}

void test_repeating_the_same_state_does_not_restart_the_delay() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    store.setState(state(0x09), 0);
    store.setState(state(0x09), 1500);
    store.tick(kIdleSaveMs);

    PresetStore reloaded(eeprom);
    reloaded.begin();
    assertState(state(0x09), reloaded.savedState());
}

void test_an_unchanged_state_is_never_written() {
    FakeEeprom eeprom;
    {
        PresetStore store(eeprom);
        store.begin();
        store.setState(state(0x33, true, 1), 0);
        store.flush();
    }
    PresetStore store(eeprom);
    store.begin();
    eeprom.resetWriteCounts();

    store.setState(state(0x33, true, 1), 100);
    store.tick(10000);
    store.flush();
    TEST_ASSERT_EQUAL_UINT32(0, eeprom.totalWrites());
}

void test_changing_back_to_the_saved_state_cancels_the_pending_save() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    store.setState(state(0x10), 0);
    store.flush();
    eeprom.resetWriteCounts();

    store.setState(state(0x20), 100);
    store.setState(state(0x10), 500);
    store.tick(100000);
    TEST_ASSERT_EQUAL_UINT32(0, eeprom.totalWrites());
}

void test_flush_saves_immediately_and_only_when_something_is_pending() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    eeprom.resetWriteCounts();
    store.flush();
    TEST_ASSERT_EQUAL_UINT32(0, eeprom.totalWrites());

    store.setState(state(0x77), 5);
    store.flush();
    PresetStore reloaded(eeprom);
    reloaded.begin();
    assertState(state(0x77), reloaded.savedState());

    const uint32_t writes = eeprom.totalWrites();
    store.tick(100000);  // nothing left to save
    TEST_ASSERT_EQUAL_UINT32(writes, eeprom.totalWrites());
}

void test_saved_state_reports_the_last_saved_record_not_the_pending_one() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    store.setState(state(0x01), 0);
    store.flush();
    store.setState(state(0x02), 10);
    assertState(state(0x01), store.savedState());
}

void test_the_delay_survives_millis_wraparound() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    store.setState(state(0x42), 0xFFFFFF00u);
    store.tick(0xFFFFFF00u + kIdleSaveMs - 1);
    PresetStore early(eeprom);
    early.begin();
    assertState(state(0x00), early.savedState());

    store.tick(0xFFFFFF00u + kIdleSaveMs);  // wraps past zero
    PresetStore late(eeprom);
    late.begin();
    assertState(state(0x42), late.savedState());
}

void test_a_state_change_saved_every_few_seconds_does_not_wear_one_cell_out() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    eeprom.resetWriteCounts();

    uint32_t now = 0;
    for (int i = 1; i <= 1000; ++i) {
        store.setState(state(static_cast<uint8_t>(i * 37), (i % 3) == 0, static_cast<uint8_t>(i % 8)), now);
        now += kIdleSaveMs;
        store.tick(now);
    }
    TEST_ASSERT_TRUE_MESSAGE(eeprom.maxWritesPerCell() <= 50, "1000 saves must spread over the 20-record ring");
}

void test_state_saves_never_touch_the_configuration() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    store.savePreset(0, "KEEP", 0x0F);
    eeprom.resetWriteCounts();

    for (int i = 0; i < 50; ++i) {
        store.setState(state(static_cast<uint8_t>(i)), 0);
        store.flush();
    }
    for (uint16_t a = 0; a < kRingAddr; ++a) TEST_ASSERT_EQUAL_UINT32(0, eeprom.writeCount(a));
    TEST_ASSERT_EQUAL_UINT32(0, eeprom.outOfRangeAccesses);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_nothing_is_written_before_the_idle_delay);
    RUN_TEST(test_the_state_is_written_once_the_idle_delay_has_passed);
    RUN_TEST(test_it_is_written_exactly_once);
    RUN_TEST(test_a_further_change_restarts_the_delay_and_only_the_last_state_is_saved);
    RUN_TEST(test_repeating_the_same_state_does_not_restart_the_delay);
    RUN_TEST(test_an_unchanged_state_is_never_written);
    RUN_TEST(test_changing_back_to_the_saved_state_cancels_the_pending_save);
    RUN_TEST(test_flush_saves_immediately_and_only_when_something_is_pending);
    RUN_TEST(test_saved_state_reports_the_last_saved_record_not_the_pending_one);
    RUN_TEST(test_the_delay_survives_millis_wraparound);
    RUN_TEST(test_a_state_change_saved_every_few_seconds_does_not_wear_one_cell_out);
    RUN_TEST(test_state_saves_never_touch_the_configuration);
    return UNITY_END();
}
