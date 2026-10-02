#include <unity.h>

#include "fake_display.h"
#include "operating_modes/menu_flow.h"
#include "operating_modes/play_screen.h"
#include "rig.h"

void setUp() {}
void tearDown() {}

struct Ui {
    explicit Ui(Rig& r)
        : flow(stack, r.store, r.controller), play(r.controller, r.store, &MenuFlow::openCallback, &flow) {
        stack.push(play);
    }

    void press(Event e, uint32_t at) {
        flow.noteInput(at);
        stack.handle(e);
    }

    ScreenStack stack;
    MenuFlow flow;
    PlayScreen play;
};

void test_nothing_changes_before_the_timeout() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    Ui ui(rig);

    ui.press(Event::Select, 1000);
    ui.flow.tick(1000 + kMenuIdleMs - 1);
    TEST_ASSERT_EQUAL_UINT8(2, ui.stack.depth());
}

void test_the_menu_closes_after_the_timeout_and_the_play_screen_returns() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    Ui ui(rig);

    ui.press(Event::Select, 1000);
    ui.press(Event::Select, 1100);  // PRESETS
    ui.press(Event::Select, 1200);  // slot menu
    TEST_ASSERT_EQUAL_UINT8(4, ui.stack.depth());

    ui.flow.tick(1200 + kMenuIdleMs);
    TEST_ASSERT_EQUAL_UINT8(1, ui.stack.depth());
    FakeDisplay display;
    ui.stack.render(display);
    TEST_ASSERT_TRUE(display.shows("MANUAL"));
}

void test_every_input_restarts_the_timer() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    Ui ui(rig);

    ui.press(Event::Select, 0);
    ui.press(Event::Right, kMenuIdleMs - 1);
    ui.flow.tick(kMenuIdleMs);  // 1 ms since the last input
    TEST_ASSERT_EQUAL_UINT8(2, ui.stack.depth());
    ui.flow.tick(kMenuIdleMs - 1 + kMenuIdleMs);
    TEST_ASSERT_EQUAL_UINT8(1, ui.stack.depth());
}

void test_a_footswitch_press_counts_as_input() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    Ui ui(rig);

    ui.press(Event::Select, 0);
    ui.flow.noteInput(kMenuIdleMs - 10);  // what the main loop does for a footswitch
    ui.flow.tick(kMenuIdleMs + 5);
    TEST_ASSERT_EQUAL_UINT8(2, ui.stack.depth());
}

void test_an_unconfirmed_text_entry_is_discarded() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    rig.store.setLoopLabel(0, "KEEP");
    Ui ui(rig);

    ui.press(Event::Select, 0);    // main menu
    ui.press(Event::Right, 10);    // LOOP NAMES
    ui.press(Event::Select, 20);   // loop list
    ui.press(Event::Select, 30);   // loop 1: text entry
    TEST_ASSERT_EQUAL_UINT8(4, ui.stack.depth());
    ui.press(Event::Back, 40);     // erase K
    ui.press(Event::Select, 50);

    ui.flow.tick(50 + kMenuIdleMs);
    TEST_ASSERT_EQUAL_UINT8(1, ui.stack.depth());
    char name[kNameMax + 1];
    rig.store.loopLabel(0, name);
    TEST_ASSERT_EQUAL_STRING("KEEP", name);
}

void test_the_play_screen_is_never_affected() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    Ui ui(rig);

    ui.flow.noteInput(0);
    ui.flow.tick(10 * kMenuIdleMs);
    TEST_ASSERT_EQUAL_UINT8(1, ui.stack.depth());
}

void test_the_menu_opens_fresh_at_the_main_menu_after_a_timeout() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    Ui ui(rig);

    ui.press(Event::Select, 0);
    ui.press(Event::Right, 10);
    ui.flow.tick(10 + kMenuIdleMs);

    ui.press(Event::Select, 100000);
    FakeDisplay display;
    ui.stack.render(display);
    TEST_ASSERT_TRUE(display.shows("PRESETS"));
    TEST_ASSERT_TRUE(display.drawn[0].inverted);  // PRESETS is highlighted again
}

void test_the_timeout_survives_millis_wraparound() {
    FakeEeprom eeprom;
    Rig rig(eeprom);
    rig.boot();
    Ui ui(rig);

    const uint32_t start = 0xFFFFFF00u;
    ui.press(Event::Select, start);
    ui.flow.tick(start + kMenuIdleMs - 1);
    TEST_ASSERT_EQUAL_UINT8(2, ui.stack.depth());
    ui.flow.tick(start + kMenuIdleMs);
    TEST_ASSERT_EQUAL_UINT8(1, ui.stack.depth());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_nothing_changes_before_the_timeout);
    RUN_TEST(test_the_menu_closes_after_the_timeout_and_the_play_screen_returns);
    RUN_TEST(test_every_input_restarts_the_timer);
    RUN_TEST(test_a_footswitch_press_counts_as_input);
    RUN_TEST(test_an_unconfirmed_text_entry_is_discarded);
    RUN_TEST(test_the_play_screen_is_never_affected);
    RUN_TEST(test_the_menu_opens_fresh_at_the_main_menu_after_a_timeout);
    RUN_TEST(test_the_timeout_survives_millis_wraparound);
    return UNITY_END();
}
