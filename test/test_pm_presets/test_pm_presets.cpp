#include <string.h>
#include <unity.h>

#include "fake_eeprom.h"
#include "preset_management/layout.h"
#include "preset_management/preset_store.h"

void setUp() {}
void tearDown() {}

static void name(const PresetStore& store, uint8_t slot, char* out) { store.presetName(slot, out); }

void test_a_fresh_store_has_no_presets() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    char buf[kNameMax + 1] = "xxx";
    for (uint8_t slot = 0; slot < kPresetCount; ++slot) {
        TEST_ASSERT_FALSE(store.presetUsed(slot));
        name(store, slot, buf);
        TEST_ASSERT_EQUAL_STRING("", buf);
        TEST_ASSERT_EQUAL_HEX8(0, store.presetMask(slot));
    }
}

void test_save_creates_a_preset_that_survives_a_power_cycle() {
    FakeEeprom eeprom;
    {
        PresetStore store(eeprom);
        store.begin();
        TEST_ASSERT_TRUE(store.savePreset(2, "CLEAN", 0b00010110));
    }
    PresetStore reloaded(eeprom);
    TEST_ASSERT_TRUE(reloaded.begin());
    char buf[kNameMax + 1];
    TEST_ASSERT_TRUE(reloaded.presetUsed(2));
    name(reloaded, 2, buf);
    TEST_ASSERT_EQUAL_STRING("CLEAN", buf);
    TEST_ASSERT_EQUAL_HEX8(0b00010110, reloaded.presetMask(2));
}

void test_a_preset_with_every_loop_off_is_still_a_preset() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    TEST_ASSERT_TRUE(store.savePreset(0, "BYPASS", 0x00));
    TEST_ASSERT_TRUE(store.presetUsed(0));
}

void test_save_overwrites_name_and_mask() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    store.savePreset(1, "LONGNAME10", 0xFF);
    TEST_ASSERT_TRUE(store.savePreset(1, "AB", 0x01));

    char buf[kNameMax + 1];
    name(store, 1, buf);
    TEST_ASSERT_EQUAL_STRING("AB", buf);
    TEST_ASSERT_EQUAL_HEX8(0x01, store.presetMask(1));
}

void test_save_rejects_bad_input_without_touching_storage() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    store.savePreset(0, "KEEP", 0x0F);
    eeprom.resetWriteCounts();

    TEST_ASSERT_FALSE(store.savePreset(0, "", 0x01));              // empty name
    TEST_ASSERT_FALSE(store.savePreset(0, "     ", 0x01));         // blank after trimming
    TEST_ASSERT_FALSE(store.savePreset(0, "ELEVENCHARS", 0x01));
    TEST_ASSERT_FALSE(store.savePreset(0, "TAB\t", 0x01));
    TEST_ASSERT_FALSE(store.savePreset(0, nullptr, 0x01));
    TEST_ASSERT_FALSE(store.savePreset(kPresetCount, "OK", 0x01));  // slot out of range
    TEST_ASSERT_EQUAL_UINT32(0, eeprom.totalWrites());

    char buf[kNameMax + 1];
    name(store, 0, buf);
    TEST_ASSERT_EQUAL_STRING("KEEP", buf);
    TEST_ASSERT_EQUAL_HEX8(0x0F, store.presetMask(0));
}

void test_trailing_spaces_are_trimmed_in_preset_names() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    store.savePreset(3, "LEAD  ", 0x03);
    char buf[kNameMax + 1];
    name(store, 3, buf);
    TEST_ASSERT_EQUAL_STRING("LEAD", buf);
}

void test_rename_changes_only_the_name() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    store.savePreset(4, "OLD", 0x55);
    TEST_ASSERT_TRUE(store.renamePreset(4, "NEW NAME"));

    char buf[kNameMax + 1];
    name(store, 4, buf);
    TEST_ASSERT_EQUAL_STRING("NEW NAME", buf);
    TEST_ASSERT_EQUAL_HEX8(0x55, store.presetMask(4));
}

void test_rename_fails_on_an_empty_slot_or_with_a_bad_name() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    TEST_ASSERT_FALSE(store.renamePreset(5, "X"));
    TEST_ASSERT_FALSE(store.presetUsed(5));

    store.savePreset(5, "KEEP", 0x01);
    TEST_ASSERT_FALSE(store.renamePreset(5, ""));
    TEST_ASSERT_FALSE(store.renamePreset(5, "ELEVENCHARS"));
    TEST_ASSERT_FALSE(store.renamePreset(kPresetCount, "X"));
    char buf[kNameMax + 1];
    name(store, 5, buf);
    TEST_ASSERT_EQUAL_STRING("KEEP", buf);
}

void test_set_mask_changes_only_the_mask() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    store.savePreset(6, "SOLO", 0x01);
    TEST_ASSERT_TRUE(store.setPresetMask(6, 0xA0));

    char buf[kNameMax + 1];
    name(store, 6, buf);
    TEST_ASSERT_EQUAL_STRING("SOLO", buf);
    TEST_ASSERT_EQUAL_HEX8(0xA0, store.presetMask(6));
}

void test_set_mask_fails_on_an_empty_slot() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    eeprom.resetWriteCounts();
    TEST_ASSERT_FALSE(store.setPresetMask(7, 0xFF));
    TEST_ASSERT_FALSE(store.setPresetMask(kPresetCount, 0xFF));
    TEST_ASSERT_EQUAL_UINT32(0, eeprom.totalWrites());
    TEST_ASSERT_FALSE(store.presetUsed(7));
}

void test_delete_empties_only_that_slot() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    store.savePreset(1, "ONE", 0x01);
    store.savePreset(2, "TWO", 0x02);
    store.savePreset(3, "THREE", 0x03);

    TEST_ASSERT_TRUE(store.deletePreset(2));
    TEST_ASSERT_FALSE(store.presetUsed(2));
    TEST_ASSERT_EQUAL_HEX8(0, store.presetMask(2));
    char buf[kNameMax + 1];
    name(store, 2, buf);
    TEST_ASSERT_EQUAL_STRING("", buf);

    TEST_ASSERT_TRUE(store.presetUsed(1));
    TEST_ASSERT_TRUE(store.presetUsed(3));
    name(store, 3, buf);
    TEST_ASSERT_EQUAL_STRING("THREE", buf);
}

void test_delete_of_an_empty_or_invalid_slot_returns_false() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    TEST_ASSERT_FALSE(store.deletePreset(0));
    TEST_ASSERT_FALSE(store.deletePreset(kPresetCount));
}

void test_a_deleted_slot_can_be_saved_again() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    store.savePreset(0, "FIRST", 0x0F);
    store.deletePreset(0);
    TEST_ASSERT_TRUE(store.savePreset(0, "SECOND", 0xF0));

    char buf[kNameMax + 1];
    name(store, 0, buf);
    TEST_ASSERT_EQUAL_STRING("SECOND", buf);
    TEST_ASSERT_EQUAL_HEX8(0xF0, store.presetMask(0));
}

void test_all_eight_presets_are_independent() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    const char* names[kPresetCount] = {"AAAAAAAAAA", "B", "CCCCCCCCCC", "D", "EEEEEEEEEE", "F", "GGGGGGGGGG", "H"};
    for (uint8_t slot = 0; slot < kPresetCount; ++slot) {
        TEST_ASSERT_TRUE(store.savePreset(slot, names[slot], static_cast<uint8_t>(1u << slot)));
    }

    PresetStore reloaded(eeprom);
    TEST_ASSERT_TRUE(reloaded.begin());
    char buf[kNameMax + 1];
    for (uint8_t slot = 0; slot < kPresetCount; ++slot) {
        name(reloaded, slot, buf);
        TEST_ASSERT_EQUAL_STRING(names[slot], buf);
        TEST_ASSERT_EQUAL_HEX8(1u << slot, reloaded.presetMask(slot));
    }
}

void test_presets_and_labels_do_not_disturb_each_other() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    store.setLoopLabel(7, "LASTLABEL");
    store.savePreset(0, "FIRST", 0xFF);
    store.savePreset(7, "LASTPRESET", 0xFF);

    char buf[kNameMax + 1];
    store.loopLabel(7, buf);
    TEST_ASSERT_EQUAL_STRING("LASTLABEL", buf);
    name(store, 7, buf);
    TEST_ASSERT_EQUAL_STRING("LASTPRESET", buf);
}

void test_unchanged_saves_cost_no_writes() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    store.savePreset(2, "SAME", 0x33);
    eeprom.resetWriteCounts();

    store.savePreset(2, "SAME", 0x33);
    store.renamePreset(2, "SAME");
    store.setPresetMask(2, 0x33);
    TEST_ASSERT_EQUAL_UINT32(0, eeprom.totalWrites());
}

void test_nothing_outside_the_configuration_region_is_written() {
    FakeEeprom eeprom;
    PresetStore store(eeprom);
    store.begin();
    for (uint8_t slot = 0; slot < kPresetCount; ++slot) store.savePreset(slot, "ABCDEFGHIJ", 0xFF);
    store.deletePreset(3);

    for (uint16_t a = kConfigEnd; a < kEepromSize; ++a) TEST_ASSERT_EQUAL_UINT32(0, eeprom.writeCount(a));
    TEST_ASSERT_EQUAL_UINT32(0, eeprom.outOfRangeAccesses);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_a_fresh_store_has_no_presets);
    RUN_TEST(test_save_creates_a_preset_that_survives_a_power_cycle);
    RUN_TEST(test_a_preset_with_every_loop_off_is_still_a_preset);
    RUN_TEST(test_save_overwrites_name_and_mask);
    RUN_TEST(test_save_rejects_bad_input_without_touching_storage);
    RUN_TEST(test_trailing_spaces_are_trimmed_in_preset_names);
    RUN_TEST(test_rename_changes_only_the_name);
    RUN_TEST(test_rename_fails_on_an_empty_slot_or_with_a_bad_name);
    RUN_TEST(test_set_mask_changes_only_the_mask);
    RUN_TEST(test_set_mask_fails_on_an_empty_slot);
    RUN_TEST(test_delete_empties_only_that_slot);
    RUN_TEST(test_delete_of_an_empty_or_invalid_slot_returns_false);
    RUN_TEST(test_a_deleted_slot_can_be_saved_again);
    RUN_TEST(test_all_eight_presets_are_independent);
    RUN_TEST(test_presets_and_labels_do_not_disturb_each_other);
    RUN_TEST(test_unchanged_saves_cost_no_writes);
    RUN_TEST(test_nothing_outside_the_configuration_region_is_written);
    return UNITY_END();
}
