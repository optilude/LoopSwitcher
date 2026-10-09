#include <string.h>
#include <unity.h>

#include "fake_display.h"
#include "operating_modes/menu_flow.h"
#include "operating_modes/play_screen.h"
#include "rig.h"

void setUp() {}
void tearDown() {}

// The play screen, menus and text entry on one stack, driven only by button events.
struct Ui {
    explicit Ui(Rig& r)
        : rig(r), flow(stack, r.store, r.controller),
          play(r.controller, r.store, &MenuFlow::openCallback, &flow) {
        stack.push(play);
    }

    void press(Event e) {
        flow.noteInput(now);
        stack.handle(e);
    }
    void presses(Event e, int n) {
        for (int i = 0; i < n; ++i) press(e);
    }
    // Moves the highlight n items down and selects.
    void choose(int n) {
        presses(Event::Right, n);
        press(Event::Select);
    }
    // Rotates the cursor from "no character" to `c` without accepting it.
    void rotateTo(char c) {
        static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -.+/#";
        presses(Event::Right, static_cast<int>(strchr(charset, c) - charset) + 1);
    }
    // Types each character with a short press.
    void type(const char* text) {
        for (const char* c = text; *c; ++c) {
            rotateTo(*c);
            press(Event::Select);
        }
    }
    void confirmText() { press(Event::SelectLong); }

    bool screenShows(const char* s) {
        FakeDisplay display;
        stack.render(display);
        return display.shows(s);
    }

    Rig& rig;
    ScreenStack stack;
    MenuFlow flow;
    PlayScreen play;
    uint32_t now = 1000;
};

static void openLoopNames(Ui& ui) {
    ui.press(Event::Select);  // play screen opens the main menu
    ui.choose(1);             // LOOP NAMES
}

static void openPresetSlot(Ui& ui, int slotIndex) {
    ui.press(Event::Select);  // main menu
    ui.choose(0);             // PRESETS
    ui.choose(slotIndex);     // the slot's own menu
}

void test_select_opens_the_menu_and_back_returns_to_the_play_screen() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    Ui ui(rig);

    TEST_ASSERT_EQUAL_UINT8(1, ui.stack.depth());
    ui.press(Event::Select);
    TEST_ASSERT_EQUAL_UINT8(2, ui.stack.depth());
    TEST_ASSERT_TRUE(ui.screenShows("PRESETS"));
    TEST_ASSERT_TRUE(ui.screenShows("LOOP NAMES"));

    ui.press(Event::Back);
    TEST_ASSERT_EQUAL_UINT8(1, ui.stack.depth());
}

void test_the_loop_list_shows_numbered_names_with_defaults() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    rig.store.setLoopLabel(1, "DELAY");
    Ui ui(rig);

    openLoopNames(ui);
    TEST_ASSERT_TRUE(ui.screenShows("1 Loop 1"));
    TEST_ASSERT_TRUE(ui.screenShows("2 DELAY"));
}

void test_labelling_a_loop_stores_it_confirms_it_and_returns_to_the_list() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    Ui ui(rig);

    openLoopNames(ui);
    ui.choose(2);  // loop 3
    TEST_ASSERT_EQUAL_UINT8(4, ui.stack.depth());
    ui.type("TS808");
    ui.confirmText();

    char name[kNameMax + 1];
    rig.store.loopLabel(2, name);
    TEST_ASSERT_EQUAL_STRING("TS808", name);
    TEST_ASSERT_EQUAL_UINT8(3, ui.stack.depth());  // back on the list
    TEST_ASSERT_TRUE(ui.screenShows("3 TS808"));
    TEST_ASSERT_TRUE(ui.screenShows("SAVED"));
}

void test_editing_an_existing_label_starts_with_its_current_text() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    rig.store.setLoopLabel(0, "FUZZ");
    Ui ui(rig);

    openLoopNames(ui);
    ui.choose(0);
    ui.confirmText();  // accept without typing

    char name[kNameMax + 1];
    rig.store.loopLabel(0, name);
    TEST_ASSERT_EQUAL_STRING("FUZZ", name);
}

void test_confirming_an_empty_text_restores_the_default_name() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    rig.store.setLoopLabel(0, "FUZZ");
    Ui ui(rig);

    openLoopNames(ui);
    ui.choose(0);
    ui.presses(Event::Back, 4);  // erase the four characters
    ui.confirmText();

    char name[kNameMax + 1];
    rig.store.loopLabel(0, name);
    TEST_ASSERT_EQUAL_STRING("Loop 1", name);
    TEST_ASSERT_FALSE(rig.store.hasLoopLabel(0));
}

void test_a_default_label_starts_the_text_entry_empty() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    Ui ui(rig);

    openLoopNames(ui);
    ui.choose(0);
    ui.confirmText();  // nothing typed

    TEST_ASSERT_FALSE(rig.store.hasLoopLabel(0));  // not "Loop 1" stored as a custom label
}

void test_cancelling_a_label_edit_changes_nothing() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    rig.store.setLoopLabel(0, "KEEP");
    Ui ui(rig);

    openLoopNames(ui);
    ui.choose(0);
    ui.type("XYZ");
    ui.press(Event::BackLong);

    char name[kNameMax + 1];
    rig.store.loopLabel(0, name);
    TEST_ASSERT_EQUAL_STRING("KEEP", name);
    TEST_ASSERT_EQUAL_UINT8(3, ui.stack.depth());
}

void test_the_preset_list_shows_numbered_names_and_empty_slots() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    rig.store.savePreset(0, "CLEAN", 0x05);
    Ui ui(rig);

    ui.press(Event::Select);
    ui.choose(0);
    TEST_ASSERT_TRUE(ui.screenShows("1 CLEAN"));
    TEST_ASSERT_TRUE(ui.screenShows("2 EMPTY"));
}

void test_saving_the_current_loops_into_an_empty_slot_asks_for_a_name_first() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    rig.controller.onFootswitch(0, 0);
    rig.controller.onFootswitch(2, 0);
    Ui ui(rig);

    openPresetSlot(ui, 1);  // slot 2
    ui.choose(0);           // SAVE LOOPS
    ui.type("CLEAN");
    ui.confirmText();

    char name[kNameMax + 1];
    TEST_ASSERT_TRUE(rig.store.presetUsed(1));
    rig.store.presetName(1, name);
    TEST_ASSERT_EQUAL_STRING("CLEAN", name);
    TEST_ASSERT_EQUAL_HEX8(0b00000101, rig.store.presetMask(1));
    TEST_ASSERT_EQUAL_UINT8(3, ui.stack.depth());  // back on the preset list
    TEST_ASSERT_TRUE(ui.screenShows("2 CLEAN"));
    TEST_ASSERT_TRUE(ui.screenShows("SAVED"));
}

void test_an_empty_preset_name_is_not_accepted() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    Ui ui(rig);

    openPresetSlot(ui, 0);
    ui.choose(0);
    ui.confirmText();  // nothing typed

    TEST_ASSERT_FALSE(rig.store.presetUsed(0));
    TEST_ASSERT_EQUAL_UINT8(5, ui.stack.depth());  // still in the text entry
    TEST_ASSERT_TRUE(ui.screenShows("NAME REQUIRED"));

    ui.type("OK");  // and the user can carry on
    ui.confirmText();
    TEST_ASSERT_TRUE(rig.store.presetUsed(0));
}

void test_overwriting_a_used_slot_asks_first_and_keeps_the_name() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    rig.store.savePreset(1, "CLEAN", 0b00000001);
    rig.controller.onFootswitch(5, 0);
    Ui ui(rig);

    openPresetSlot(ui, 1);
    ui.choose(0);  // SAVE LOOPS: a confirmation, defaulting to NO
    TEST_ASSERT_TRUE(ui.screenShows("OVERWRITE?"));
    ui.press(Event::Select);  // NO
    TEST_ASSERT_EQUAL_HEX8(0b00000001, rig.store.presetMask(1));
    TEST_ASSERT_EQUAL_UINT8(4, ui.stack.depth());  // back on the slot menu

    ui.choose(0);
    ui.press(Event::Left);  // YES
    ui.press(Event::Select);
    char name[kNameMax + 1];
    rig.store.presetName(1, name);
    TEST_ASSERT_EQUAL_STRING("CLEAN", name);
    TEST_ASSERT_EQUAL_HEX8(0b00100000, rig.store.presetMask(1));
    TEST_ASSERT_EQUAL_UINT8(3, ui.stack.depth());
    TEST_ASSERT_TRUE(ui.screenShows("SAVED"));
}

void test_renaming_changes_only_the_name() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    rig.store.savePreset(2, "OLD", 0b00001111);
    Ui ui(rig);

    openPresetSlot(ui, 2);
    ui.choose(1);              // RENAME
    ui.presses(Event::Back, 3);  // erase OLD
    ui.type("NEW");
    ui.confirmText();

    char name[kNameMax + 1];
    rig.store.presetName(2, name);
    TEST_ASSERT_EQUAL_STRING("NEW", name);
    TEST_ASSERT_EQUAL_HEX8(0b00001111, rig.store.presetMask(2));
    TEST_ASSERT_EQUAL_UINT8(3, ui.stack.depth());
    TEST_ASSERT_TRUE(ui.screenShows("RENAMED"));
}

void test_rename_starts_with_the_current_name_and_an_empty_result_is_refused() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    rig.store.savePreset(0, "AB", 0x01);
    Ui ui(rig);

    openPresetSlot(ui, 0);
    ui.choose(1);
    ui.presses(Event::Back, 2);
    ui.confirmText();  // empty
    TEST_ASSERT_TRUE(ui.screenShows("NAME REQUIRED"));
    char name[kNameMax + 1];
    rig.store.presetName(0, name);
    TEST_ASSERT_EQUAL_STRING("AB", name);
}

void test_an_empty_slot_menu_offers_only_save_loops() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    Ui ui(rig);

    openPresetSlot(ui, 4);
    TEST_ASSERT_TRUE(ui.screenShows("SAVE LOOPS"));
    TEST_ASSERT_FALSE(ui.screenShows("RENAME"));
    TEST_ASSERT_FALSE(ui.screenShows("DELETE"));

    // There is nothing else to move to: selecting still means SAVE LOOPS.
    ui.choose(2);
    TEST_ASSERT_EQUAL_UINT8(5, ui.stack.depth());  // the name entry of SAVE LOOPS
}

void test_a_used_slot_menu_offers_save_rename_and_delete() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    rig.store.savePreset(4, "USED", 0x01);
    Ui ui(rig);

    openPresetSlot(ui, 4);
    TEST_ASSERT_TRUE(ui.screenShows("SAVE LOOPS"));
    TEST_ASSERT_TRUE(ui.screenShows("RENAME"));
    TEST_ASSERT_TRUE(ui.screenShows("DELETE"));
}

#if LOOPSWITCHER_ENABLE_MIDI
void test_midi_menu_edits_channel_and_toggles_effect_cc() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    rig.store.savePreset(0, "LEAD", 0x01);
    Ui ui(rig);

    openPresetSlot(ui, 0);
    ui.choose(3);
    TEST_ASSERT_EQUAL_UINT8(5, ui.stack.depth());
    TEST_ASSERT_TRUE(ui.screenShows("CHANNEL 1"));
    ui.choose(0);
    TEST_ASSERT_TRUE(ui.screenShows("CHANNEL"));
    ui.presses(Event::Right, 3);
    ui.press(Event::Select);
    TEST_ASSERT_EQUAL_UINT8(3, rig.store.presetMidi(0).channel);
    TEST_ASSERT_EQUAL_UINT8(5, ui.stack.depth());  // back on the MIDI menu

    ui.choose(6);
    TEST_ASSERT_TRUE(rig.store.presetMidi(0).effectCcEnabled);
    TEST_ASSERT_TRUE(ui.screenShows("EFFECT CC ON"));
}

void test_midi_program_editor_steps_the_value_and_wraps() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    rig.store.savePreset(0, "LEAD", 0x01);
    Ui ui(rig);

    openPresetSlot(ui, 0);
    ui.choose(3);
    ui.choose(5);  // PROGRAM, starting at 1
    TEST_ASSERT_TRUE(ui.screenShows("1"));
    ui.press(Event::Left);  // wraps to 128
    TEST_ASSERT_TRUE(ui.screenShows("128"));
    ui.press(Event::Select);
    TEST_ASSERT_EQUAL_UINT8(127, rig.store.presetMidi(0).program);
}

void test_cancelling_a_midi_value_edit_changes_nothing() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    rig.store.savePreset(0, "LEAD", 0x01);
    Ui ui(rig);

    openPresetSlot(ui, 0);
    ui.choose(3);
    ui.choose(8);  // CC VALUE
    ui.presses(Event::Right, 10);
    ui.press(Event::Back);
    TEST_ASSERT_EQUAL_UINT8(0, rig.store.presetMidi(0).effectValue);
    TEST_ASSERT_EQUAL_UINT8(5, ui.stack.depth());
}
#endif

void test_the_slot_menu_shows_rename_and_delete_once_a_preset_has_been_saved_into_it() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    Ui ui(rig);

    openPresetSlot(ui, 0);
    ui.choose(0);
    ui.type("NEW");
    ui.confirmText();  // back on the preset list
    ui.choose(0);      // open slot 1 again
    TEST_ASSERT_TRUE(ui.screenShows("RENAME"));
    TEST_ASSERT_TRUE(ui.screenShows("DELETE"));
}

void test_the_slot_menu_loses_rename_and_delete_after_the_preset_is_deleted() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    rig.store.savePreset(0, "GONE", 0x03);
    Ui ui(rig);

    openPresetSlot(ui, 0);
    ui.choose(2);
    ui.press(Event::Left);
    ui.press(Event::Select);  // deleted
    ui.choose(0);             // open slot 1 again
    TEST_ASSERT_TRUE(ui.screenShows("SAVE LOOPS"));
    TEST_ASSERT_FALSE(ui.screenShows("RENAME"));
    TEST_ASSERT_FALSE(ui.screenShows("DELETE"));
}

void test_the_last_character_of_a_name_can_be_confirmed_with_just_a_long_press() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    Ui ui(rig);

    openLoopNames(ui);
    ui.choose(0);
    ui.type("TS80");
    ui.rotateTo('8');  // the last character is only rotated to
    ui.confirmText();  // and saved with the long press

    char name[kNameMax + 1];
    rig.store.loopLabel(0, name);
    TEST_ASSERT_EQUAL_STRING("TS808", name);
}

void test_a_preset_name_can_be_a_single_character_and_a_long_press() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    Ui ui(rig);

    openPresetSlot(ui, 0);
    ui.choose(0);
    ui.rotateTo('Z');
    ui.confirmText();

    char name[kNameMax + 1];
    rig.store.presetName(0, name);
    TEST_ASSERT_EQUAL_STRING("Z", name);
}

void test_deleting_asks_first_and_empties_the_slot() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    rig.store.savePreset(0, "KEEP", 0x03);
    rig.store.savePreset(1, "GONE", 0x0C);
    Ui ui(rig);

    openPresetSlot(ui, 1);
    ui.choose(2);  // DELETE
    TEST_ASSERT_TRUE(ui.screenShows("DELETE?"));
    ui.press(Event::Select);  // NO is the default
    TEST_ASSERT_TRUE(rig.store.presetUsed(1));

    ui.press(Event::Select);  // reopen DELETE, which remains highlighted
    ui.press(Event::Left);  // YES
    ui.press(Event::Select);
    TEST_ASSERT_FALSE(rig.store.presetUsed(1));
    TEST_ASSERT_TRUE(rig.store.presetUsed(0));
    TEST_ASSERT_EQUAL_UINT8(3, ui.stack.depth());
    TEST_ASSERT_TRUE(ui.screenShows("2 EMPTY"));
    TEST_ASSERT_TRUE(ui.screenShows("DELETED"));
}

void test_back_at_the_delete_confirmation_means_no() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    rig.store.savePreset(0, "KEEP", 0x03);
    Ui ui(rig);

    openPresetSlot(ui, 0);
    ui.choose(2);
    ui.press(Event::Left);  // YES highlighted ...
    ui.press(Event::Back);  // ... but Back means NO
    TEST_ASSERT_TRUE(rig.store.presetUsed(0));
}

void test_deleting_the_active_preset_leaves_the_loops_and_shows_empty() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    rig.store.savePreset(0, "CLEAN", 0b00000101);
    rig.controller.toggleMode(0);  // preset mode applies preset 1
    Ui ui(rig);

    openPresetSlot(ui, 0);
    ui.choose(2);
    ui.press(Event::Left);
    ui.press(Event::Select);
    ui.presses(Event::Back, 3);  // out of the menus

    TEST_ASSERT_EQUAL_UINT8(1, ui.stack.depth());
    TEST_ASSERT_EQUAL_HEX8(0b00000101, rig.loops.stateMask());
    TEST_ASSERT_TRUE(ui.screenShows("EMPTY"));
}

void test_back_walks_out_of_every_level() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    Ui ui(rig);

    openPresetSlot(ui, 0);
    TEST_ASSERT_EQUAL_UINT8(4, ui.stack.depth());
    ui.press(Event::Back);
    TEST_ASSERT_EQUAL_UINT8(3, ui.stack.depth());
    ui.press(Event::Back);
    TEST_ASSERT_EQUAL_UINT8(2, ui.stack.depth());
    ui.press(Event::Back);
    TEST_ASSERT_EQUAL_UINT8(1, ui.stack.depth());
}

void test_the_menu_always_opens_with_the_first_item_highlighted() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    Ui ui(rig);

    ui.press(Event::Select);
    ui.press(Event::Right);  // LOOP NAMES highlighted
    ui.press(Event::Back);
    ui.press(Event::Select);
    ui.press(Event::Select);  // PRESETS again, not LOOP NAMES, if the highlight was reset
    TEST_ASSERT_TRUE(ui.screenShows("1 EMPTY"));
    TEST_ASSERT_FALSE(ui.screenShows("1 Loop 1"));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_select_opens_the_menu_and_back_returns_to_the_play_screen);
    RUN_TEST(test_the_loop_list_shows_numbered_names_with_defaults);
    RUN_TEST(test_labelling_a_loop_stores_it_confirms_it_and_returns_to_the_list);
    RUN_TEST(test_editing_an_existing_label_starts_with_its_current_text);
    RUN_TEST(test_confirming_an_empty_text_restores_the_default_name);
    RUN_TEST(test_a_default_label_starts_the_text_entry_empty);
    RUN_TEST(test_cancelling_a_label_edit_changes_nothing);
    RUN_TEST(test_the_preset_list_shows_numbered_names_and_empty_slots);
    RUN_TEST(test_saving_the_current_loops_into_an_empty_slot_asks_for_a_name_first);
    RUN_TEST(test_an_empty_preset_name_is_not_accepted);
    RUN_TEST(test_overwriting_a_used_slot_asks_first_and_keeps_the_name);
    RUN_TEST(test_renaming_changes_only_the_name);
    RUN_TEST(test_rename_starts_with_the_current_name_and_an_empty_result_is_refused);
    RUN_TEST(test_an_empty_slot_menu_offers_only_save_loops);
    RUN_TEST(test_a_used_slot_menu_offers_save_rename_and_delete);
#if LOOPSWITCHER_ENABLE_MIDI
    RUN_TEST(test_midi_menu_edits_channel_and_toggles_effect_cc);
    RUN_TEST(test_midi_program_editor_steps_the_value_and_wraps);
    RUN_TEST(test_cancelling_a_midi_value_edit_changes_nothing);
#endif
    RUN_TEST(test_the_slot_menu_shows_rename_and_delete_once_a_preset_has_been_saved_into_it);
    RUN_TEST(test_the_slot_menu_loses_rename_and_delete_after_the_preset_is_deleted);
    RUN_TEST(test_the_last_character_of_a_name_can_be_confirmed_with_just_a_long_press);
    RUN_TEST(test_a_preset_name_can_be_a_single_character_and_a_long_press);
    RUN_TEST(test_deleting_asks_first_and_empties_the_slot);
    RUN_TEST(test_back_at_the_delete_confirmation_means_no);
    RUN_TEST(test_deleting_the_active_preset_leaves_the_loops_and_shows_empty);
    RUN_TEST(test_back_walks_out_of_every_level);
    RUN_TEST(test_the_menu_always_opens_with_the_first_item_highlighted);
    return UNITY_END();
}
