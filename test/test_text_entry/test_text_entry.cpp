#include <string.h>
#include <unity.h>

#include "fake_display.h"
#include "ui_framework/text_entry.h"

static char result[kTextMax + 1];
static int doneCalls, cancelCalls;

static void onDone(void*, const char* text) {
    strcpy(result, text);
    ++doneCalls;
}
static void onCancel(void*) { ++cancelCalls; }

void setUp() {
    result[0] = '\0';
    doneCalls = cancelCalls = 0;
}
void tearDown() {}

// Rotates right until the cursor shows `c`, then accepts it with a short press.
static void rotateTo(TextEntry& entry, char c) {
    for (int guard = 0; guard < 100 && entry.workingChar() != c; ++guard) entry.handle(Event::Right);
    TEST_ASSERT_EQUAL_CHAR(c, entry.workingChar());
}
static void type(TextEntry& entry, const char* text) {
    for (const char* c = text; *c; ++c) {
        rotateTo(entry, *c);
        entry.handle(Event::Select);
    }
}

void test_the_cursor_starts_on_no_character_and_rotating_cycles_the_character_set() {
    TextEntry entry(onDone, onCancel, nullptr);
    TEST_ASSERT_FALSE(entry.hasPendingChar());
    entry.handle(Event::Right);
    TEST_ASSERT_EQUAL_CHAR('A', entry.workingChar());
    entry.handle(Event::Right);
    TEST_ASSERT_EQUAL_CHAR('B', entry.workingChar());
    entry.handle(Event::Left);
    entry.handle(Event::Left);
    TEST_ASSERT_FALSE(entry.hasPendingChar());  // back on "no character"
    entry.handle(Event::Left);
    TEST_ASSERT_EQUAL_CHAR('#', entry.workingChar());  // wrapped to the last character
    entry.handle(Event::Right);
    TEST_ASSERT_FALSE(entry.hasPendingChar());
}

void test_character_set_contains_exactly_the_agreed_characters() {
    TextEntry entry(onDone, onCancel, nullptr);
    char seen[64] = {};
    int n = 0;
    entry.handle(Event::Right);
    while (entry.hasPendingChar() && n < 63) {
        seen[n++] = entry.workingChar();
        entry.handle(Event::Right);
    }
    TEST_ASSERT_EQUAL_STRING("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 -.+/#", seen);
}

void test_a_short_press_accepts_the_character_and_clears_the_cursor() {
    TextEntry entry(onDone, onCancel, nullptr);
    type(entry, "TS");
    TEST_ASSERT_EQUAL_STRING("TS", entry.text());
    TEST_ASSERT_FALSE(entry.hasPendingChar());
}

void test_a_short_press_with_no_character_does_nothing() {
    TextEntry entry(onDone, onCancel, nullptr);
    TEST_ASSERT_TRUE(entry.handle(Event::Select));
    TEST_ASSERT_EQUAL_STRING("", entry.text());
}

void test_a_long_press_saves_the_character_under_the_cursor_too() {
    TextEntry entry(onDone, onCancel, nullptr);
    type(entry, "FUZ");
    rotateTo(entry, 'Z');  // the last character is only rotated to, never short-pressed
    entry.handle(Event::SelectLong);

    TEST_ASSERT_EQUAL_INT(1, doneCalls);
    TEST_ASSERT_EQUAL_STRING("FUZZ", result);
    TEST_ASSERT_EQUAL_INT(0, cancelCalls);
}

void test_a_long_press_right_after_a_short_press_adds_nothing() {
    TextEntry entry(onDone, onCancel, nullptr);
    type(entry, "FUZZ");  // every character short-pressed, as before
    entry.handle(Event::SelectLong);
    TEST_ASSERT_EQUAL_STRING("FUZZ", result);
}

void test_the_last_character_can_be_an_A() {
    TextEntry entry(onDone, onCancel, nullptr);
    type(entry, "B");
    rotateTo(entry, 'A');
    entry.handle(Event::SelectLong);
    TEST_ASSERT_EQUAL_STRING("BA", result);
}

void test_confirming_empty_text_reports_an_empty_string() {
    TextEntry entry(onDone, onCancel, nullptr);
    entry.handle(Event::SelectLong);
    TEST_ASSERT_EQUAL_INT(1, doneCalls);
    TEST_ASSERT_EQUAL_STRING("", result);
}

void test_a_single_character_can_be_saved_with_one_rotation_and_a_long_press() {
    TextEntry entry(onDone, onCancel, nullptr);
    rotateTo(entry, 'X');
    entry.handle(Event::SelectLong);
    TEST_ASSERT_EQUAL_STRING("X", result);
}

void test_confirming_does_not_change_the_edit_state_so_it_can_continue_after_a_refusal() {
    TextEntry entry(onDone, onCancel, nullptr);
    type(entry, "AB");
    rotateTo(entry, 'C');
    entry.handle(Event::SelectLong);

    TEST_ASSERT_EQUAL_STRING("AB", entry.text());          // not yet accepted
    TEST_ASSERT_EQUAL_CHAR('C', entry.workingChar());
    entry.handle(Event::Select);
    TEST_ASSERT_EQUAL_STRING("ABC", entry.text());
}

void test_text_is_limited_to_ten_characters() {
    TextEntry entry(onDone, onCancel, nullptr);
    type(entry, "ABCDEFGHIJ");
    TEST_ASSERT_EQUAL_STRING("ABCDEFGHIJ", entry.text());

    entry.handle(Event::Select);  // nothing to accept
    entry.handle(Event::Right);   // and no character to pick
    TEST_ASSERT_FALSE(entry.hasPendingChar());
    entry.handle(Event::SelectLong);
    TEST_ASSERT_EQUAL_STRING("ABCDEFGHIJ", result);
}

void test_the_tenth_character_can_be_the_pending_one() {
    TextEntry entry(onDone, onCancel, nullptr);
    type(entry, "ABCDEFGHI");
    rotateTo(entry, 'J');
    entry.handle(Event::SelectLong);
    TEST_ASSERT_EQUAL_STRING("ABCDEFGHIJ", result);
}

void test_back_clears_a_pending_character_before_erasing_accepted_ones() {
    TextEntry entry(onDone, onCancel, nullptr);
    type(entry, "AB");
    rotateTo(entry, 'C');

    TEST_ASSERT_TRUE(entry.handle(Event::Back));
    TEST_ASSERT_FALSE(entry.hasPendingChar());
    TEST_ASSERT_EQUAL_STRING("AB", entry.text());
    TEST_ASSERT_TRUE(entry.handle(Event::Back));
    TEST_ASSERT_EQUAL_STRING("A", entry.text());
    TEST_ASSERT_EQUAL_INT(0, cancelCalls);
}

void test_erasing_then_confirming_saves_the_shorter_text() {
    TextEntry entry(onDone, onCancel, nullptr);
    type(entry, "FUZZ");
    entry.handle(Event::Back);
    entry.handle(Event::SelectLong);
    TEST_ASSERT_EQUAL_STRING("FUZ", result);
}

void test_back_on_empty_text_with_nothing_pending_cancels() {
    TextEntry entry(onDone, onCancel, nullptr);
    entry.handle(Event::Back);
    TEST_ASSERT_EQUAL_INT(1, cancelCalls);
    TEST_ASSERT_EQUAL_INT(0, doneCalls);
}

void test_back_on_empty_text_with_a_pending_character_clears_it_instead_of_cancelling() {
    TextEntry entry(onDone, onCancel, nullptr);
    rotateTo(entry, 'Q');
    entry.handle(Event::Back);
    TEST_ASSERT_EQUAL_INT(0, cancelCalls);
    TEST_ASSERT_FALSE(entry.hasPendingChar());
    entry.handle(Event::Back);
    TEST_ASSERT_EQUAL_INT(1, cancelCalls);
}

void test_long_back_cancels_immediately() {
    TextEntry entry(onDone, onCancel, nullptr);
    type(entry, "XYZ");
    rotateTo(entry, 'Q');
    entry.handle(Event::BackLong);
    TEST_ASSERT_EQUAL_INT(1, cancelCalls);
    TEST_ASSERT_EQUAL_INT(0, doneCalls);
}

void test_reset_loads_initial_text_and_clears_the_cursor() {
    TextEntry entry(onDone, onCancel, nullptr);
    type(entry, "OLD");
    rotateTo(entry, 'Z');
    entry.reset("TS808");
    TEST_ASSERT_EQUAL_STRING("TS808", entry.text());
    TEST_ASSERT_FALSE(entry.hasPendingChar());
    entry.handle(Event::SelectLong);  // opened and confirmed without a change
    TEST_ASSERT_EQUAL_STRING("TS808", result);
    entry.reset("");
    TEST_ASSERT_EQUAL_STRING("", entry.text());
}

void test_reset_truncates_overlong_initial_text() {
    TextEntry entry(onDone, onCancel, nullptr);
    entry.reset("ABCDEFGHIJKLMN");
    TEST_ASSERT_EQUAL_STRING("ABCDEFGHIJ", entry.text());
}

void test_draw_shows_text_and_an_inverted_cursor_character() {
    TextEntry entry(onDone, onCancel, nullptr);
    type(entry, "TS");
    rotateTo(entry, 'B');
    FakeDisplay display;
    entry.draw(display);

    bool text = false, cursor = false;
    for (const Drawn& d : display.drawn) {
        if (d.text == "TS" && d.x == 0 && d.font == Font::Status) text = true;
        if (d.text == "B" && d.x == 2 * kStatusCharWidth && d.inverted) cursor = true;
    }
    TEST_ASSERT_TRUE(text);
    TEST_ASSERT_TRUE(cursor);
}

void test_draw_shows_an_underscore_cursor_when_no_character_is_chosen() {
    TextEntry entry(onDone, onCancel, nullptr);
    type(entry, "TS");
    FakeDisplay display;
    entry.draw(display);

    bool cursor = false;
    for (const Drawn& d : display.drawn) {
        if (d.text == "_" && d.x == 2 * kStatusCharWidth && d.inverted) cursor = true;
    }
    TEST_ASSERT_TRUE(cursor);
}

void test_draw_has_no_cursor_when_the_text_is_full() {
    TextEntry entry(onDone, onCancel, nullptr);
    entry.reset("ABCDEFGHIJ");
    FakeDisplay display;
    entry.draw(display);
    for (const Drawn& d : display.drawn) TEST_ASSERT_FALSE(d.inverted);
}

void test_other_events_are_not_used() {
    TextEntry entry(onDone, onCancel, nullptr);
    TEST_ASSERT_FALSE(entry.handle(Event::Mode));
    TEST_ASSERT_FALSE(entry.handle(Event::ModeLong));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_the_cursor_starts_on_no_character_and_rotating_cycles_the_character_set);
    RUN_TEST(test_character_set_contains_exactly_the_agreed_characters);
    RUN_TEST(test_a_short_press_accepts_the_character_and_clears_the_cursor);
    RUN_TEST(test_a_short_press_with_no_character_does_nothing);
    RUN_TEST(test_a_long_press_saves_the_character_under_the_cursor_too);
    RUN_TEST(test_a_long_press_right_after_a_short_press_adds_nothing);
    RUN_TEST(test_the_last_character_can_be_an_A);
    RUN_TEST(test_confirming_empty_text_reports_an_empty_string);
    RUN_TEST(test_a_single_character_can_be_saved_with_one_rotation_and_a_long_press);
    RUN_TEST(test_confirming_does_not_change_the_edit_state_so_it_can_continue_after_a_refusal);
    RUN_TEST(test_text_is_limited_to_ten_characters);
    RUN_TEST(test_the_tenth_character_can_be_the_pending_one);
    RUN_TEST(test_back_clears_a_pending_character_before_erasing_accepted_ones);
    RUN_TEST(test_erasing_then_confirming_saves_the_shorter_text);
    RUN_TEST(test_back_on_empty_text_with_nothing_pending_cancels);
    RUN_TEST(test_back_on_empty_text_with_a_pending_character_clears_it_instead_of_cancelling);
    RUN_TEST(test_long_back_cancels_immediately);
    RUN_TEST(test_reset_loads_initial_text_and_clears_the_cursor);
    RUN_TEST(test_reset_truncates_overlong_initial_text);
    RUN_TEST(test_draw_shows_text_and_an_inverted_cursor_character);
    RUN_TEST(test_draw_shows_an_underscore_cursor_when_no_character_is_chosen);
    RUN_TEST(test_draw_has_no_cursor_when_the_text_is_full);
    RUN_TEST(test_other_events_are_not_used);
    return UNITY_END();
}
