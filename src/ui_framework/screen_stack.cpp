#include "ui_framework/screen_stack.h"

bool ScreenStack::push(Screen& screen) {
    if (depth_ == kMaxDepth) return false;
    screens_[depth_++] = &screen;
    dirty_ = true;
    return true;
}

void ScreenStack::pop() {
    if (depth_ <= 1) return;
    --depth_;
    dirty_ = true;
}

void ScreenStack::popToRoot() {
    if (depth_ <= 1) return;
    depth_ = 1;
    dirty_ = true;
}

void ScreenStack::handle(Event event) {
    if (event == Event::None || depth_ == 0) return;
    const bool used = screens_[depth_ - 1]->handle(event);
    if (!used && event == Event::Back) pop();
    dirty_ = true;
}

void ScreenStack::showToast(const char* message, uint32_t nowMs, uint32_t durationMs) {
    uint8_t i = 0;
    for (; i < kToastMaxChars && message[i] != '\0'; ++i) toast_[i] = message[i];
    toast_[i] = '\0';
    toastUntil_ = nowMs + durationMs;
    toastActive_ = true;
    dirty_ = true;
}

void ScreenStack::tick(uint32_t nowMs) {
    if (toastActive_ && static_cast<int32_t>(nowMs - toastUntil_) >= 0) {
        toastActive_ = false;
        dirty_ = true;
    }
}

bool ScreenStack::renderStep(Display& display) {
    if (depth_ == 0) return false;
    if (!frameActive_) {
        if (!dirty_) return false;
        display.firstPage();
        frameActive_ = true;
        dirty_ = false;
    }
    screens_[depth_ - 1]->draw(display);
    if (toastActive_) display.text(0, kScreenHeight - kRowHeight, toast_, Font::List, true);
    frameActive_ = display.nextPage();
    return frameActive_;
}

void ScreenStack::render(Display& display) {
    if (depth_ == 0) return;
    dirty_ = true;
    while (renderStep(display)) {
    }
}
