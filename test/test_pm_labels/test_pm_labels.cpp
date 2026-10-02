#include <string.h>
#include <unity.h>

#include "fake_eeprom.h"
#include "preset_management/crc16.h"
#include "preset_management/layout.h"
#include "preset_management/preset_store.h"

void setUp() {}
void tearDown() {}

static void label(const PresetStore& store, uint8_t loop, char* out) { store.loopLabel(loop, out); }

static void writeValidConfig(FakeEeprom& eeprom) {
    PresetStore store(eeprom);
    store.begin();
}

void test_blank_eeprom_loads_defaults_and_reports_it() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    TEST_ASSERT_FALSE(store.begin());

    char name[kNameMax + 1];
    for (uint8_t i = 0; i < kLabelCount; ++i) {
        label(store, i, name);
        char expected[8] = {'L', 'o', 'o', 'p', ' ', static_cast<char>('1' + i), 0};
        TEST_ASSERT_EQUAL_STRING(expected, name);
    }
}

void test_defaults_are_written_so_the_next_boot_is_clean() {
    FakeEeprom eeprom;
    PresetStore first(eeprom);
    TEST_ASSERT_FALSE(first.begin());

    PresetStore second(eeprom);
    TEST_ASSERT_TRUE(second.begin());
    TEST_ASSERT_EQUAL_HEX8(kLayoutVersion, eeprom.read(kVersionAddr));
}

void test_wrong_layout_version_resets_to_defaults() {
    FakeEeprom eeprom;
    writeValidConfig(eeprom);
    PresetStore store(eeprom);
    store.setLoopLabel(0, "FUZZ");
    eeprom.update(kVersionAddr, kLayoutVersion + 1);

    PresetStore reloaded(eeprom);
    TEST_ASSERT_FALSE(reloaded.begin());
    char name[kNameMax + 1];
    label(reloaded, 0, name);
    TEST_ASSERT_EQUAL_STRING("Loop 1", name);
}

void test_corrupted_configuration_resets_to_defaults() {
    FakeEeprom eeprom;
    writeValidConfig(eeprom);
    PresetStore store(eeprom);
    store.setLoopLabel(2, "DELAY");
    eeprom.update(kLabelsAddr + 2 * kNameMax + 1, 'X');  // damage one byte behind the CRC's back

    PresetStore reloaded(eeprom);
    TEST_ASSERT_FALSE(reloaded.begin());
    char name[kNameMax + 1];
    label(reloaded, 2, name);
    TEST_ASSERT_EQUAL_STRING("Loop 3", name);
}

void test_label_survives_a_power_cycle() {
    FakeEeprom eeprom;
    {
        PresetStore store(eeprom);
        store.begin();
        TEST_ASSERT_TRUE(store.setLoopLabel(4, "TS808"));
    }
    PresetStore reloaded(eeprom);
    TEST_ASSERT_TRUE(reloaded.begin());
    char name[kNameMax + 1];
    label(reloaded, 4, name);
    TEST_ASSERT_EQUAL_STRING("TS808", name);
}

void test_a_full_length_label_round_trips() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    TEST_ASSERT_TRUE(store.setLoopLabel(7, "ABCDEFGHIJ"));

    PresetStore reloaded(eeprom);
    TEST_ASSERT_TRUE(reloaded.begin());
    char name[kNameMax + 1];
    label(reloaded, 7, name);
    TEST_ASSERT_EQUAL_STRING("ABCDEFGHIJ", name);
}

void test_labels_do_not_overlap_each_other() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    store.setLoopLabel(2, "AAAAAAAAAA");
    store.setLoopLabel(4, "BBBBBBBBBB");
    store.setLoopLabel(3, "CC");

    char name[kNameMax + 1];
    label(store, 2, name);
    TEST_ASSERT_EQUAL_STRING("AAAAAAAAAA", name);
    label(store, 3, name);
    TEST_ASSERT_EQUAL_STRING("CC", name);
    label(store, 4, name);
    TEST_ASSERT_EQUAL_STRING("BBBBBBBBBB", name);
}

void test_trailing_spaces_are_trimmed_but_inner_and_leading_spaces_stay() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    char name[kNameMax + 1];

    TEST_ASSERT_TRUE(store.setLoopLabel(0, "TS808   "));
    label(store, 0, name);
    TEST_ASSERT_EQUAL_STRING("TS808", name);

    TEST_ASSERT_TRUE(store.setLoopLabel(1, " BIG MUFF"));
    label(store, 1, name);
    TEST_ASSERT_EQUAL_STRING(" BIG MUFF", name);
}

void test_empty_or_blank_label_restores_the_default_name() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    store.setLoopLabel(5, "REVERB");
    char name[kNameMax + 1];

    TEST_ASSERT_TRUE(store.setLoopLabel(5, ""));
    label(store, 5, name);
    TEST_ASSERT_EQUAL_STRING("Loop 6", name);

    store.setLoopLabel(5, "REVERB");
    TEST_ASSERT_TRUE(store.setLoopLabel(5, "   "));
    label(store, 5, name);
    TEST_ASSERT_EQUAL_STRING("Loop 6", name);
}

void test_invalid_names_are_rejected_without_touching_storage() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    store.setLoopLabel(0, "KEEP");
    eeprom.resetWriteCounts();

    TEST_ASSERT_FALSE(store.setLoopLabel(0, "ELEVENCHARS"));    // 11 characters
    TEST_ASSERT_FALSE(store.setLoopLabel(0, "TAB\there"));      // control character
    TEST_ASSERT_FALSE(store.setLoopLabel(0, "DEL\x7f"));         // 0x7F
    TEST_ASSERT_FALSE(store.setLoopLabel(0, "HIGH\xc3\xa9"));    // non-ASCII bytes
    TEST_ASSERT_FALSE(store.setLoopLabel(0, nullptr));
    TEST_ASSERT_FALSE(store.setLoopLabel(kLabelCount, "OK"));   // loop out of range
    TEST_ASSERT_EQUAL_UINT32(0, eeprom.totalWrites());

    char name[kNameMax + 1];
    label(store, 0, name);
    TEST_ASSERT_EQUAL_STRING("KEEP", name);
}

void test_only_changed_bytes_are_written() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    store.setLoopLabel(1, "DELAY");

    eeprom.resetWriteCounts();
    store.setLoopLabel(1, "DELAY");  // identical
    TEST_ASSERT_EQUAL_UINT32(0, eeprom.totalWrites());

    store.setLoopLabel(1, "DELAX");  // one character differs, plus the CRC
    TEST_ASSERT_TRUE(eeprom.totalWrites() <= 3);
    TEST_ASSERT_EQUAL_UINT32(1, eeprom.writeCount(kLabelsAddr + kNameMax + 4));
}

void test_out_of_range_loop_reads_as_empty_text() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    char name[kNameMax + 1] = "xxx";
    label(store, kLabelCount, name);
    TEST_ASSERT_EQUAL_STRING("", name);
    TEST_ASSERT_EQUAL_UINT32(0, eeprom.outOfRangeAccesses);
}

void test_nothing_outside_the_configuration_region_is_written() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    for (uint8_t i = 0; i < kLabelCount; ++i) store.setLoopLabel(i, "ABCDEFGHIJ");

    for (uint16_t a = kConfigEnd; a < kEepromSize; ++a) TEST_ASSERT_EQUAL_UINT32(0, eeprom.writeCount(a));
    TEST_ASSERT_EQUAL_UINT32(0, eeprom.outOfRangeAccesses);
}

void test_has_loop_label_tells_a_custom_label_from_the_default() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    TEST_ASSERT_FALSE(store.hasLoopLabel(2));
    store.setLoopLabel(2, "FUZZ");
    TEST_ASSERT_TRUE(store.hasLoopLabel(2));
    store.setLoopLabel(2, "");
    TEST_ASSERT_FALSE(store.hasLoopLabel(2));
    TEST_ASSERT_FALSE(store.hasLoopLabel(kLabelCount));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_has_loop_label_tells_a_custom_label_from_the_default);
    RUN_TEST(test_blank_eeprom_loads_defaults_and_reports_it);
    RUN_TEST(test_defaults_are_written_so_the_next_boot_is_clean);
    RUN_TEST(test_wrong_layout_version_resets_to_defaults);
    RUN_TEST(test_corrupted_configuration_resets_to_defaults);
    RUN_TEST(test_label_survives_a_power_cycle);
    RUN_TEST(test_a_full_length_label_round_trips);
    RUN_TEST(test_labels_do_not_overlap_each_other);
    RUN_TEST(test_trailing_spaces_are_trimmed_but_inner_and_leading_spaces_stay);
    RUN_TEST(test_empty_or_blank_label_restores_the_default_name);
    RUN_TEST(test_invalid_names_are_rejected_without_touching_storage);
    RUN_TEST(test_only_changed_bytes_are_written);
    RUN_TEST(test_out_of_range_loop_reads_as_empty_text);
    RUN_TEST(test_nothing_outside_the_configuration_region_is_written);
    return UNITY_END();
}
