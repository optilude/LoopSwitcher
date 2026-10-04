#include <unity.h>

#include "fake_eeprom.h"
#include "preset_management/layout.h"
#include "preset_management/preset_store.h"
#include "preset_management/state_ring.h"

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

static uint16_t slotAddr(uint8_t slot) { return kRingAddr + slot * kRingRecordSize; }

void test_perform_mode_round_trips_and_conflicts_with_preset_mode() {
    FakeEeprom eeprom;
    StateRing ring(eeprom);
    ring.scan();
    ring.append(SavedState{0x3C, false, 6, true});

    StateRing reloaded(eeprom);
    reloaded.scan();
    SavedState out{};
    TEST_ASSERT_TRUE(reloaded.load(out));
    TEST_ASSERT_TRUE(out.performMode);
    TEST_ASSERT_FALSE(out.presetMode);
    TEST_ASSERT_EQUAL_UINT8(6, out.activePreset);

    const uint8_t seq = 9, mask = 0x01, modePreset = 0xC0;  // preset and perform together
    eeprom.update(slotAddr(7) + 0, seq);
    eeprom.update(slotAddr(7) + 1, mask);
    eeprom.update(slotAddr(7) + 2, modePreset);
    eeprom.update(slotAddr(7) + 3, static_cast<uint8_t>(seq ^ mask ^ modePreset ^ 0xA5));
    StateRing bad(eeprom);
    bad.scan();
    TEST_ASSERT_TRUE(bad.load(out));
    TEST_ASSERT_EQUAL_HEX8(0x3C, out.loopMask);  // the impossible record was skipped
}

void test_an_erased_ring_has_no_record() {
    FakeEeprom eeprom;
    StateRing ring(eeprom);
    ring.scan();
    SavedState out = state(0x12);
    TEST_ASSERT_FALSE(ring.load(out));
}

void test_a_saved_state_round_trips_through_a_power_cycle() {
    FakeEeprom eeprom;
    {
        StateRing ring(eeprom);
        ring.scan();
        ring.append(state(0xA5, true, 5));
    }
    StateRing reloaded(eeprom);
    reloaded.scan();
    SavedState out{};
    TEST_ASSERT_TRUE(reloaded.load(out));
    assertState(state(0xA5, true, 5), out);
}

void test_the_newest_record_wins() {
    FakeEeprom eeprom;
    StateRing ring(eeprom);
    ring.scan();
    ring.append(state(0x01));
    ring.append(state(0x02));
    ring.append(state(0x03, true, 2));

    StateRing reloaded(eeprom);
    reloaded.scan();
    SavedState out{};
    TEST_ASSERT_TRUE(reloaded.load(out));
    assertState(state(0x03, true, 2), out);
}

void test_successive_records_advance_through_all_slots_and_wrap() {
    FakeEeprom eeprom;
    StateRing ring(eeprom);
    ring.scan();
    for (uint8_t i = 0; i < kRingRecords; ++i) ring.append(state(i));
    for (uint8_t slot = 0; slot < kRingRecords; ++slot) {
        TEST_ASSERT_EQUAL_HEX8(slot, eeprom.read(slotAddr(slot) + 1));  // loop mask written to slot i
        TEST_ASSERT_EQUAL_UINT8(slot, eeprom.read(slotAddr(slot)));     // sequence number
    }

    ring.append(state(0xEE));  // the 21st record replaces slot 0
    TEST_ASSERT_EQUAL_HEX8(0xEE, eeprom.read(slotAddr(0) + 1));
    TEST_ASSERT_EQUAL_HEX8(0x01, eeprom.read(slotAddr(1) + 1));
}

void test_the_newest_record_is_found_across_sequence_wraparound() {
    FakeEeprom eeprom;
    StateRing ring(eeprom);
    ring.scan();
    for (int i = 0; i < 600; ++i) {
        ring.append(state(static_cast<uint8_t>(i), (i & 1) != 0, static_cast<uint8_t>(i & 7)));

        StateRing reloaded(eeprom);
        reloaded.scan();
        SavedState out{};
        TEST_ASSERT_TRUE(reloaded.load(out));
        assertState(state(static_cast<uint8_t>(i), (i & 1) != 0, static_cast<uint8_t>(i & 7)), out);
    }
}

void test_a_record_with_a_bad_checksum_is_skipped() {
    FakeEeprom eeprom;
    StateRing ring(eeprom);
    ring.scan();
    ring.append(state(0x11));
    ring.append(state(0x22));
    eeprom.update(slotAddr(1) + 3, static_cast<uint8_t>(eeprom.read(slotAddr(1) + 3) ^ 0x01));

    StateRing reloaded(eeprom);
    reloaded.scan();
    SavedState out{};
    TEST_ASSERT_TRUE(reloaded.load(out));
    assertState(state(0x11), out);
}

void test_a_record_with_an_impossible_mode_byte_is_rejected_even_with_a_valid_checksum() {
    FakeEeprom eeprom;
    const uint8_t seq = 7, mask = 0x33, modePreset = 0x08;  // bit 3 is unused
    eeprom.update(slotAddr(4) + 0, seq);
    eeprom.update(slotAddr(4) + 1, mask);
    eeprom.update(slotAddr(4) + 2, modePreset);
    eeprom.update(slotAddr(4) + 3, static_cast<uint8_t>(seq ^ mask ^ modePreset ^ 0xA5));

    StateRing ring(eeprom);
    ring.scan();
    SavedState out{};
    TEST_ASSERT_FALSE(ring.load(out));
}

void test_appending_after_a_reboot_continues_after_the_newest_record() {
    FakeEeprom eeprom;
    {
        StateRing ring(eeprom);
        ring.scan();
        for (uint8_t i = 0; i < 5; ++i) ring.append(state(i));
    }
    StateRing reloaded(eeprom);
    reloaded.scan();
    reloaded.append(state(0x77));

    TEST_ASSERT_EQUAL_HEX8(0x77, eeprom.read(slotAddr(5) + 1));
    TEST_ASSERT_EQUAL_UINT8(5, eeprom.read(slotAddr(5)));
}

void test_wear_is_spread_evenly() {
    FakeEeprom eeprom;
    StateRing ring(eeprom);
    ring.scan();
    for (int i = 0; i < 1000; ++i) ring.append(state(static_cast<uint8_t>(i * 37), (i % 3) == 0, static_cast<uint8_t>(i % 8)));

    TEST_ASSERT_TRUE_MESSAGE(eeprom.maxWritesPerCell() <= 50, "no cell may be written more than 1000/20 times");
}

void test_nothing_outside_the_ring_is_written() {
    FakeEeprom eeprom;
    StateRing ring(eeprom);
    ring.scan();
    for (int i = 0; i < 100; ++i) ring.append(state(static_cast<uint8_t>(i)));

    for (uint16_t a = 0; a < kRingAddr; ++a) TEST_ASSERT_EQUAL_UINT32(0, eeprom.writeCount(a));
    TEST_ASSERT_EQUAL_UINT32(0, eeprom.outOfRangeAccesses);
}

void test_the_store_returns_the_default_state_when_nothing_was_saved() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    assertState(state(0x00, false, 0), store.savedState());
}

void test_resetting_the_configuration_keeps_the_saved_state() {
    FakeEeprom eeprom;
    StateRing ring(eeprom);
    ring.scan();
    ring.append(state(0x5A, true, 3));

    PresetStore store(eeprom);
    TEST_ASSERT_FALSE(store.begin());  // blank configuration is reset
    assertState(state(0x5A, true, 3), store.savedState());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_an_erased_ring_has_no_record);
    RUN_TEST(test_perform_mode_round_trips_and_conflicts_with_preset_mode);
    RUN_TEST(test_a_saved_state_round_trips_through_a_power_cycle);
    RUN_TEST(test_the_newest_record_wins);
    RUN_TEST(test_successive_records_advance_through_all_slots_and_wrap);
    RUN_TEST(test_the_newest_record_is_found_across_sequence_wraparound);
    RUN_TEST(test_a_record_with_a_bad_checksum_is_skipped);
    RUN_TEST(test_a_record_with_an_impossible_mode_byte_is_rejected_even_with_a_valid_checksum);
    RUN_TEST(test_appending_after_a_reboot_continues_after_the_newest_record);
    RUN_TEST(test_wear_is_spread_evenly);
    RUN_TEST(test_nothing_outside_the_ring_is_written);
    RUN_TEST(test_the_store_returns_the_default_state_when_nothing_was_saved);
    RUN_TEST(test_resetting_the_configuration_keeps_the_saved_state);
    return UNITY_END();
}
