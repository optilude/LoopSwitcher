#pragma once

#include <stdint.h>

#include "operating_modes/performance_controller.h"
#include "preset_management/preset_store.h"
#include "ui_framework/confirm.h"
#include "ui_framework/menu.h"
#include "ui_framework/screen_stack.h"
#include "ui_framework/text_entry.h"

constexpr uint32_t kMenuIdleMs = 60000;

// The menus for naming loops and for saving, renaming and deleting presets, built from the
// ui-framework widgets. The play screen opens it with openCallback().
class MenuFlow {
public:
    MenuFlow(ScreenStack& stack, PresetStore& store, PerformanceController& controller);

    // For PlayScreen's open-menu hook; `context` is the MenuFlow.
    static void openCallback(void* context) { static_cast<MenuFlow*>(context)->openMainMenu(); }

    void openMainMenu();

    // Call for every input event and footswitch press: the time stamps toasts and restarts the
    // idle timer.
    void noteInput(uint32_t nowMs) { lastInputMs_ = nowMs; }

    // Closes every menu and edit screen (discarding unconfirmed text) after kMenuIdleMs without
    // input, so a pedal left in a menu on stage returns to the play screen.
    void tick(uint32_t nowMs);

private:
    static constexpr uint8_t kRowTextSize = kNameMax + 3;  // "n " + name + terminator
    static constexpr uint8_t kSlotMenuItems = 3;           // SAVE LOOPS, RENAME, DELETE

    struct ItemContext {
        MenuFlow* flow;
        uint8_t index;
    };
    enum class Edit : uint8_t { LoopLabel, NewPreset, RenamePreset };

    void openPresetList();
    void openLoopList();
    void openSlotMenu(uint8_t slot);
    void refreshLoopList();
    void refreshPresetList();
    void startEdit(Edit kind, const char* initialText);
    void closeSlotMenuAfter(const char* toast);
    void toast(const char* message);

    void onTextDone(const char* text);
    void onOverwriteChoice(bool yes);
    void onDeleteChoice(bool yes);

    static void onMainPresets(void* context);
    static void onMainLoops(void* context);
    static void onLoopSelected(void* context);
    static void onPresetSelected(void* context);
    static void onSlotSave(void* context);
    static void onSlotRename(void* context);
    static void onSlotDelete(void* context);
    static void textDone(void* context, const char* text);
    static void textCancel(void* context);
    static void overwriteChoice(void* context, bool yes);
    static void deleteChoice(void* context, bool yes);

    ScreenStack& stack_;
    PresetStore& store_;
    PerformanceController& controller_;

    MenuItem mainItems_[2];
    MenuItem loopItems_[kLabelCount];
    MenuItem presetItems_[kPresetCount];
    MenuItem slotItems_[kSlotMenuItems];
    ItemContext loopContexts_[kLabelCount];
    ItemContext presetContexts_[kPresetCount];
    char loopText_[kLabelCount][kRowTextSize];
    char presetText_[kPresetCount][kRowTextSize];

    Menu mainMenu_{nullptr, 0};
    Menu loopMenu_{nullptr, 0};
    Menu presetMenu_{nullptr, 0};
    Menu slotMenu_{nullptr, 0};
    TextEntry textEntry_;
    Confirm overwriteConfirm_;
    Confirm deleteConfirm_;

    Edit edit_ = Edit::LoopLabel;
    uint8_t editIndex_ = 0;  // the loop or preset slot being edited
    uint32_t lastInputMs_ = 0;
};
