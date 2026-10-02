#include <unity.h>

#include "fake_display.h"
#include "ui_framework/menu.h"

static int selectedIndex = -1;
static void onSelect(void* context) { selectedIndex = *static_cast<int*>(context); }

void setUp() { selectedIndex = -1; }
void tearDown() {}

static int ids[8] = {0, 1, 2, 3, 4, 5, 6, 7};

static void makeItems(MenuItem* items, int count) {
    static const char* labels[8] = {"ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT"};
    for (int i = 0; i < count; ++i) items[i] = {labels[i], true, onSelect, &ids[i]};
}

void test_highlight_starts_on_first_item_and_moves_without_wrapping() {
    MenuItem items[3];
    makeItems(items, 3);
    Menu menu(items, 3);

    TEST_ASSERT_EQUAL_UINT8(0, menu.highlight());
    menu.handle(Event::Left);
    TEST_ASSERT_EQUAL_UINT8(0, menu.highlight());
    menu.handle(Event::Right);
    menu.handle(Event::Right);
    menu.handle(Event::Right);
    TEST_ASSERT_EQUAL_UINT8(2, menu.highlight());
    menu.handle(Event::Left);
    TEST_ASSERT_EQUAL_UINT8(1, menu.highlight());
}

void test_select_runs_the_callback_of_the_highlighted_item() {
    MenuItem items[3];
    makeItems(items, 3);
    Menu menu(items, 3);

    menu.handle(Event::Right);
    TEST_ASSERT_TRUE(menu.handle(Event::Select));
    TEST_ASSERT_EQUAL_INT(1, selectedIndex);
}

void test_disabled_items_are_skipped_and_cannot_be_selected() {
    MenuItem items[4];
    makeItems(items, 4);
    items[0].enabled = false;
    items[2].enabled = false;
    Menu menu(items, 4);

    TEST_ASSERT_EQUAL_UINT8(1, menu.highlight());  // first enabled item
    menu.handle(Event::Right);
    TEST_ASSERT_EQUAL_UINT8(3, menu.highlight());
    menu.handle(Event::Left);
    TEST_ASSERT_EQUAL_UINT8(1, menu.highlight());
    menu.handle(Event::Left);
    TEST_ASSERT_EQUAL_UINT8(1, menu.highlight());
}

void test_select_does_nothing_when_no_item_is_enabled() {
    MenuItem items[2];
    makeItems(items, 2);
    items[0].enabled = false;
    items[1].enabled = false;
    Menu menu(items, 2);

    menu.handle(Event::Select);
    TEST_ASSERT_EQUAL_INT(-1, selectedIndex);
}

void test_back_and_other_events_are_not_used() {
    MenuItem items[2];
    makeItems(items, 2);
    Menu menu(items, 2);

    TEST_ASSERT_FALSE(menu.handle(Event::Back));
    TEST_ASSERT_FALSE(menu.handle(Event::Mode));
    TEST_ASSERT_FALSE(menu.handle(Event::SelectLong));
}

void test_window_scrolls_to_keep_the_highlight_visible() {
    MenuItem items[8];
    makeItems(items, 8);
    Menu menu(items, 8);
    FakeDisplay display;

    menu.draw(display);
    TEST_ASSERT_EQUAL_UINT(4, display.drawn.size());
    TEST_ASSERT_EQUAL_STRING("ONE", display.drawn[0].text.c_str());

    for (int i = 0; i < 5; ++i) menu.handle(Event::Right);  // highlight on item 5 (SIX)
    display.drawn.clear();
    menu.draw(display);
    TEST_ASSERT_EQUAL_UINT(4, display.drawn.size());
    TEST_ASSERT_EQUAL_STRING("THREE", display.drawn[0].text.c_str());
    TEST_ASSERT_EQUAL_STRING("SIX", display.drawn[3].text.c_str());
    TEST_ASSERT_TRUE(display.drawn[3].inverted);
    TEST_ASSERT_FALSE(display.drawn[0].inverted);

    for (int i = 0; i < 5; ++i) menu.handle(Event::Left);  // back to the top
    display.drawn.clear();
    menu.draw(display);
    TEST_ASSERT_EQUAL_STRING("ONE", display.drawn[0].text.c_str());
}

void test_rows_are_laid_out_top_to_bottom() {
    MenuItem items[3];
    makeItems(items, 3);
    Menu menu(items, 3);
    FakeDisplay display;

    menu.draw(display);
    TEST_ASSERT_EQUAL_UINT(3, display.drawn.size());
    for (int i = 0; i < 3; ++i) TEST_ASSERT_EQUAL_UINT8(i * kRowHeight, display.drawn[i].y);
}

void test_setItems_keeps_the_highlight_in_range() {
    MenuItem items[5];
    makeItems(items, 5);
    Menu menu(items, 5);
    for (int i = 0; i < 4; ++i) menu.handle(Event::Right);
    TEST_ASSERT_EQUAL_UINT8(4, menu.highlight());

    menu.setItems(items, 2);
    TEST_ASSERT_EQUAL_UINT8(1, menu.highlight());
}

void test_reset_highlight_returns_to_the_first_enabled_item_and_the_top() {
    MenuItem items[8];
    makeItems(items, 8);
    items[0].enabled = false;
    Menu menu(items, 8);
    for (int i = 0; i < 6; ++i) menu.handle(Event::Right);
    TEST_ASSERT_EQUAL_UINT8(7, menu.highlight());

    menu.resetHighlight();
    TEST_ASSERT_EQUAL_UINT8(1, menu.highlight());
    FakeDisplay display;
    menu.draw(display);
    TEST_ASSERT_EQUAL_STRING("ONE", display.drawn[0].text.c_str());  // scrolled to the top; disabled items still show
    TEST_ASSERT_TRUE(display.drawn[1].inverted);                     // the highlight is on TWO
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_reset_highlight_returns_to_the_first_enabled_item_and_the_top);
    RUN_TEST(test_highlight_starts_on_first_item_and_moves_without_wrapping);
    RUN_TEST(test_select_runs_the_callback_of_the_highlighted_item);
    RUN_TEST(test_disabled_items_are_skipped_and_cannot_be_selected);
    RUN_TEST(test_select_does_nothing_when_no_item_is_enabled);
    RUN_TEST(test_back_and_other_events_are_not_used);
    RUN_TEST(test_window_scrolls_to_keep_the_highlight_visible);
    RUN_TEST(test_rows_are_laid_out_top_to_bottom);
    RUN_TEST(test_setItems_keeps_the_highlight_in_range);
    return UNITY_END();
}
