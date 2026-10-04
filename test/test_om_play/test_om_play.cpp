#include <unity.h>

#include "fake_display.h"
#include "operating_modes/play_screen.h"
#include "rig.h"

static int menuOpened;
static void openMenu(void*) { ++menuOpened; }

void setUp() { menuOpened = 0; }
void tearDown() {}

struct Screens {
    explicit Screens(Rig& rig) : screen(rig.controller, rig.store, openMenu, nullptr) {}
    PlayScreen screen;
    FakeDisplay display;
};

static void savePresets(Rig& rig) {
    rig.store.savePreset(0, "CLEAN", 0b00000101);
    rig.store.savePreset(2, "LEAD", 0b11110000);
}

void test_manual_mode_before_any_change_shows_only_the_mode() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    Screens s(rig);

    s.screen.draw(s.display);
    TEST_ASSERT_EQUAL_UINT(1, s.display.drawn.size());
    TEST_ASSERT_TRUE(s.display.shows("MANUAL"));
}

void test_manual_mode_shows_the_last_changed_loop_name_and_its_state() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    rig.store.setLoopLabel(3, "TS808");
    Screens s(rig);

    rig.controller.onFootswitch(3, 0);
    s.screen.draw(s.display);
    TEST_ASSERT_TRUE(s.display.shows("MANUAL"));
    TEST_ASSERT_TRUE(s.display.shows("TS808"));
    TEST_ASSERT_TRUE(s.display.shows("ON"));

    s.display.drawn.clear();
    rig.controller.onFootswitch(3, 0);
    s.screen.draw(s.display);
    TEST_ASSERT_TRUE(s.display.shows("OFF"));
    TEST_ASSERT_FALSE(s.display.shows("ON"));
}

void test_a_loop_without_a_label_shows_its_default_name() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    Screens s(rig);

    rig.controller.onFootswitch(5, 0);
    s.screen.draw(s.display);
    TEST_ASSERT_TRUE(s.display.shows("Loop 6"));
}

void test_the_loop_name_uses_the_large_font() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    Screens s(rig);

    rig.controller.onFootswitch(0, 0);
    s.screen.draw(s.display);
    for (const Drawn& d : s.display.drawn) {
        if (d.text == "Loop 1") TEST_ASSERT_EQUAL(Font::Status, d.font);
    }
}

void test_preset_mode_shows_the_slot_number_and_name() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    savePresets(rig);
    Screens s(rig);

    rig.controller.toggleMode(0);
    rig.controller.onFootswitch(2, 0);
    s.screen.draw(s.display);
    TEST_ASSERT_TRUE(s.display.shows("PRESET 3"));
    TEST_ASSERT_TRUE(s.display.shows("LEAD"));
    TEST_ASSERT_FALSE(s.display.shows("MANUAL"));
}

void test_preset_mode_with_an_empty_active_slot_shows_empty() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    Screens s(rig);

    rig.controller.toggleMode(0);
    s.screen.draw(s.display);
    TEST_ASSERT_TRUE(s.display.shows("PRESET 1"));
    TEST_ASSERT_TRUE(s.display.shows("EMPTY"));
}

void test_deleting_the_active_preset_makes_the_screen_show_empty() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    savePresets(rig);
    Screens s(rig);
    rig.controller.toggleMode(0);
    rig.store.deletePreset(0);

    s.screen.draw(s.display);
    TEST_ASSERT_TRUE(s.display.shows("EMPTY"));
}

void test_leaving_preset_mode_does_not_leave_a_stale_loop_on_the_screen() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    savePresets(rig);
    Screens s(rig);

    rig.controller.onFootswitch(3, 0);  // manual: loop 4 on
    rig.controller.toggleMode(0);       // preset 1 moves the relays
    rig.controller.toggleMode(0);       // perform
    rig.controller.toggleMode(0);       // back to manual
    s.screen.draw(s.display);
    TEST_ASSERT_TRUE(s.display.shows("MANUAL"));
    TEST_ASSERT_EQUAL_UINT(1, s.display.drawn.size());  // no stale loop name
}

void test_select_opens_the_menu_and_back_is_left_alone() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    Screens s(rig);

    TEST_ASSERT_TRUE(s.screen.handle(Event::Select));
    TEST_ASSERT_EQUAL_INT(1, menuOpened);
    TEST_ASSERT_FALSE(s.screen.handle(Event::Back));
    TEST_ASSERT_FALSE(s.screen.handle(Event::Left));
    TEST_ASSERT_EQUAL_INT(1, menuOpened);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_manual_mode_before_any_change_shows_only_the_mode);
    RUN_TEST(test_manual_mode_shows_the_last_changed_loop_name_and_its_state);
    RUN_TEST(test_a_loop_without_a_label_shows_its_default_name);
    RUN_TEST(test_the_loop_name_uses_the_large_font);
    RUN_TEST(test_preset_mode_shows_the_slot_number_and_name);
    RUN_TEST(test_preset_mode_with_an_empty_active_slot_shows_empty);
    RUN_TEST(test_deleting_the_active_preset_makes_the_screen_show_empty);
    RUN_TEST(test_leaving_preset_mode_does_not_leave_a_stale_loop_on_the_screen);
    RUN_TEST(test_select_opens_the_menu_and_back_is_left_alone);
    return UNITY_END();
}
