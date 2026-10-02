#include <unity.h>

#include "fake_display.h"
#include "ui_framework/screen_stack.h"

class TestScreen : public Screen {
public:
    explicit TestScreen(const char* name) : name(name) {}
    const char* name;
    bool consumeBack = false;
    int draws = 0;
    int counter = 0;
    Event lastEvent = Event::None;

    bool handle(Event event) override {
        lastEvent = event;
        if (event == Event::Select) ++counter;
        return event == Event::Back ? consumeBack : true;
    }
    void draw(Display& display) override {
        ++draws;
        display.text(0, 0, name, Font::List, false);
    }
};

void setUp() {}
void tearDown() {}

void test_unused_back_pops_but_never_the_root() {
    ScreenStack stack;
    TestScreen root("root"), child("child");
    stack.push(root);
    stack.push(child);

    stack.handle(Event::Back);
    TEST_ASSERT_EQUAL_UINT8(1, stack.depth());
    stack.handle(Event::Back);
    TEST_ASSERT_EQUAL_UINT8(1, stack.depth());
}

void test_screen_that_uses_back_stays_on_the_stack() {
    ScreenStack stack;
    TestScreen root("root"), child("child");
    child.consumeBack = true;
    stack.push(root);
    stack.push(child);

    stack.handle(Event::Back);
    TEST_ASSERT_EQUAL_UINT8(2, stack.depth());
}

void test_events_go_to_the_top_screen_and_state_survives_push_and_pop() {
    ScreenStack stack;
    TestScreen root("root"), child("child");
    stack.push(root);
    stack.handle(Event::Select);
    stack.push(child);
    stack.handle(Event::Select);
    stack.handle(Event::Select);
    stack.handle(Event::Back);

    TEST_ASSERT_EQUAL_INT(1, root.counter);
    TEST_ASSERT_EQUAL_INT(2, child.counter);
    stack.handle(Event::Select);
    TEST_ASSERT_EQUAL_INT(2, root.counter);
}

void test_push_is_rejected_when_full() {
    ScreenStack stack;
    TestScreen s("s");
    for (int i = 0; i < ScreenStack::kMaxDepth; ++i) TEST_ASSERT_TRUE(stack.push(s));
    TEST_ASSERT_FALSE(stack.push(s));
    TEST_ASSERT_EQUAL_UINT8(ScreenStack::kMaxDepth, stack.depth());
}

void test_redraw_only_when_dirty() {
    ScreenStack stack;
    FakeDisplay display;
    TestScreen root("root");
    stack.push(root);

    TEST_ASSERT_TRUE(stack.needsRender());
    stack.render(display);
    TEST_ASSERT_FALSE(stack.needsRender());

    stack.handle(Event::None);
    TEST_ASSERT_FALSE(stack.needsRender());
    stack.handle(Event::Select);
    TEST_ASSERT_TRUE(stack.needsRender());
    stack.render(display);
    stack.markDirty();
    TEST_ASSERT_TRUE(stack.needsRender());
}

void test_draw_runs_once_per_display_pass() {
    ScreenStack stack;
    FakeDisplay display;
    display.passesPerFrame = 8;
    TestScreen root("root");
    stack.push(root);

    stack.render(display);
    TEST_ASSERT_EQUAL_INT(8, root.draws);
    TEST_ASSERT_EQUAL_INT(8, display.lastFramePasses);
    TEST_ASSERT_TRUE(display.shows("root"));
}

void test_toast_shows_for_one_second_then_clears() {
    ScreenStack stack;
    FakeDisplay display;
    TestScreen root("root");
    stack.push(root);
    stack.render(display);

    stack.showToast("SAVED", 5000);
    TEST_ASSERT_TRUE(stack.needsRender());
    stack.render(display);
    TEST_ASSERT_TRUE(display.shows("SAVED"));
    TEST_ASSERT_TRUE(display.drawn.back().inverted);

    stack.tick(5999);
    TEST_ASSERT_FALSE(stack.needsRender());
    stack.tick(6000);
    TEST_ASSERT_TRUE(stack.needsRender());
    stack.render(display);
    TEST_ASSERT_FALSE(display.shows("SAVED"));
}

void test_toast_is_truncated_to_its_limit() {
    ScreenStack stack;
    FakeDisplay display;
    TestScreen root("root");
    stack.push(root);

    stack.showToast("ABCDEFGHIJKLMNOPQRSTUVWXYZ", 0);
    stack.render(display);
    TEST_ASSERT_TRUE(display.shows("ABCDEFGHIJKLMNOP"));  // kToastMaxChars = 16
    TEST_ASSERT_FALSE(display.shows("ABCDEFGHIJKLMNOPQ"));
}

void test_render_step_draws_one_pass_per_call_until_the_frame_is_done() {
    ScreenStack stack;
    FakeDisplay display;
    display.passesPerFrame = 8;
    TestScreen root("root");
    stack.push(root);

    for (int step = 1; step <= 7; ++step) {
        TEST_ASSERT_TRUE(stack.renderStep(display));
        TEST_ASSERT_EQUAL_INT(step, root.draws);
        TEST_ASSERT_TRUE(stack.needsRender());
    }
    TEST_ASSERT_FALSE(stack.renderStep(display));  // eighth and last pass
    TEST_ASSERT_EQUAL_INT(8, root.draws);
    TEST_ASSERT_FALSE(stack.needsRender());
    TEST_ASSERT_EQUAL_INT(1, display.framesStarted);
}

void test_render_step_does_nothing_when_nothing_changed() {
    ScreenStack stack;
    FakeDisplay display;
    TestScreen root("root");
    stack.push(root);
    stack.render(display);

    TEST_ASSERT_FALSE(stack.renderStep(display));
    TEST_ASSERT_EQUAL_INT(1, display.framesStarted);
    TEST_ASSERT_EQUAL_INT(1, root.draws);
}

void test_a_change_during_a_frame_is_drawn_in_a_following_frame() {
    ScreenStack stack;
    FakeDisplay display;
    display.passesPerFrame = 4;
    TestScreen root("root");
    stack.push(root);

    stack.renderStep(display);
    stack.renderStep(display);
    stack.handle(Event::Select);  // marks the screen dirty mid-frame
    stack.renderStep(display);
    TEST_ASSERT_FALSE(stack.renderStep(display));  // first frame finishes
    TEST_ASSERT_TRUE(stack.needsRender());          // a second frame is pending

    for (int i = 0; i < 3; ++i) TEST_ASSERT_TRUE(stack.renderStep(display));
    TEST_ASSERT_FALSE(stack.renderStep(display));
    TEST_ASSERT_EQUAL_INT(2, display.framesStarted);
    TEST_ASSERT_EQUAL_INT(8, root.draws);
    TEST_ASSERT_FALSE(stack.needsRender());
}

void test_toast_is_drawn_on_every_pass_of_a_stepped_frame() {
    ScreenStack stack;
    FakeDisplay display;
    display.passesPerFrame = 3;
    TestScreen root("root");
    stack.push(root);
    stack.showToast("SAVED", 0);

    for (int i = 0; i < 3; ++i) stack.renderStep(display);
    for (size_t pass = 0; pass < 3; ++pass) TEST_ASSERT_TRUE(display.passShows(pass, "SAVED"));
}

void test_pop_to_root_leaves_only_the_first_screen_and_marks_it_dirty() {
    ScreenStack stack;
    FakeDisplay display;
    TestScreen root("root"), a("a"), b("b");
    stack.push(root);
    stack.push(a);
    stack.push(b);
    stack.render(display);

    stack.popToRoot();
    TEST_ASSERT_EQUAL_UINT8(1, stack.depth());
    TEST_ASSERT_TRUE(stack.needsRender());
    stack.render(display);
    TEST_ASSERT_TRUE(display.shows("root"));

    stack.popToRoot();  // already at the root: harmless
    TEST_ASSERT_EQUAL_UINT8(1, stack.depth());
}

void test_a_toast_can_be_given_its_own_duration() {
    ScreenStack stack;
    FakeDisplay display;
    TestScreen root("root");
    stack.push(root);

    stack.showToast("DATA RESET", 5000, 2000);
    stack.tick(5000 + kToastMs);  // the default duration would have ended here
    stack.render(display);
    TEST_ASSERT_TRUE(display.shows("DATA RESET"));

    stack.tick(7000);
    stack.render(display);
    TEST_ASSERT_FALSE(display.shows("DATA RESET"));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_a_toast_can_be_given_its_own_duration);
    RUN_TEST(test_pop_to_root_leaves_only_the_first_screen_and_marks_it_dirty);
    RUN_TEST(test_render_step_draws_one_pass_per_call_until_the_frame_is_done);
    RUN_TEST(test_render_step_does_nothing_when_nothing_changed);
    RUN_TEST(test_a_change_during_a_frame_is_drawn_in_a_following_frame);
    RUN_TEST(test_toast_is_drawn_on_every_pass_of_a_stepped_frame);
    RUN_TEST(test_unused_back_pops_but_never_the_root);
    RUN_TEST(test_screen_that_uses_back_stays_on_the_stack);
    RUN_TEST(test_events_go_to_the_top_screen_and_state_survives_push_and_pop);
    RUN_TEST(test_push_is_rejected_when_full);
    RUN_TEST(test_redraw_only_when_dirty);
    RUN_TEST(test_draw_runs_once_per_display_pass);
    RUN_TEST(test_toast_shows_for_one_second_then_clears);
    RUN_TEST(test_toast_is_truncated_to_its_limit);
    return UNITY_END();
}
