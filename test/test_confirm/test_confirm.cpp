#include <unity.h>

#include "fake_display.h"
#include "ui_framework/confirm.h"

static int calls;
static bool lastChoice;
static void onChoice(void*, bool yes) {
    ++calls;
    lastChoice = yes;
}

void setUp() {
    calls = 0;
    lastChoice = true;
}
void tearDown() {}

void test_default_highlight_is_no_so_a_stray_press_does_not_confirm() {
    Confirm confirm("DELETE?", onChoice, nullptr);
    confirm.handle(Event::Select);
    TEST_ASSERT_EQUAL_INT(1, calls);
    TEST_ASSERT_FALSE(lastChoice);
}

void test_left_selects_yes_and_right_selects_no() {
    Confirm confirm("DELETE?", onChoice, nullptr);
    confirm.handle(Event::Left);
    confirm.handle(Event::Select);
    TEST_ASSERT_TRUE(lastChoice);

    confirm.reset();
    confirm.handle(Event::Left);
    confirm.handle(Event::Right);
    confirm.handle(Event::Select);
    TEST_ASSERT_FALSE(lastChoice);
}

void test_back_means_no() {
    Confirm confirm("DELETE?", onChoice, nullptr);
    confirm.handle(Event::Left);  // YES highlighted
    TEST_ASSERT_TRUE(confirm.handle(Event::Back));
    TEST_ASSERT_EQUAL_INT(1, calls);
    TEST_ASSERT_FALSE(lastChoice);
}

void test_reset_returns_the_highlight_to_no() {
    Confirm confirm("DELETE?", onChoice, nullptr);
    confirm.handle(Event::Left);
    confirm.reset();
    confirm.handle(Event::Select);
    TEST_ASSERT_FALSE(lastChoice);
}

void test_draw_shows_question_and_highlights_the_choice() {
    Confirm confirm("OVERWRITE?", onChoice, nullptr);
    FakeDisplay display;
    confirm.draw(display);

    bool question = false, noInverted = false, yesInverted = false;
    for (const Drawn& d : display.drawn) {
        if (d.text == "OVERWRITE?") question = true;
        if (d.text == "NO" && d.inverted) noInverted = true;
        if (d.text == "YES" && d.inverted) yesInverted = true;
    }
    TEST_ASSERT_TRUE(question);
    TEST_ASSERT_TRUE(noInverted);
    TEST_ASSERT_FALSE(yesInverted);
}

void test_other_events_are_not_used() {
    Confirm confirm("DELETE?", onChoice, nullptr);
    TEST_ASSERT_FALSE(confirm.handle(Event::Mode));
    TEST_ASSERT_FALSE(confirm.handle(Event::SelectLong));
    TEST_ASSERT_EQUAL_INT(0, calls);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_default_highlight_is_no_so_a_stray_press_does_not_confirm);
    RUN_TEST(test_left_selects_yes_and_right_selects_no);
    RUN_TEST(test_back_means_no);
    RUN_TEST(test_reset_returns_the_highlight_to_no);
    RUN_TEST(test_draw_shows_question_and_highlights_the_choice);
    RUN_TEST(test_other_events_are_not_used);
    return UNITY_END();
}
