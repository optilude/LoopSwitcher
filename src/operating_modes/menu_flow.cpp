#include "operating_modes/menu_flow.h"

#include <stdio.h>

namespace {

// "n " followed by `name`, for the list rows.
void rowText(char* out, uint8_t index, const char* name) {
    out[0] = static_cast<char>('1' + index);
    out[1] = ' ';
    uint8_t i = 0;
    for (; name[i] != '\0' && i < kNameMax; ++i) out[2 + i] = name[i];
    out[2 + i] = '\0';
}

}  // namespace

MenuFlow::MenuFlow(ScreenStack& stack, PresetStore& store, PerformanceController& controller)
    : stack_(stack),
      store_(store),
      controller_(controller),
#if LOOPSWITCHER_ENABLE_MIDI
      numberEntry_(&MenuFlow::numberDone, &MenuFlow::popScreen, this),
#endif
      textEntry_(&MenuFlow::textDone, &MenuFlow::popScreen, this),
      overwriteConfirm_("OVERWRITE?", &MenuFlow::overwriteChoice, this),
      deleteConfirm_("DELETE?", &MenuFlow::deleteChoice, this) {
    mainItems_[0] = {"PRESETS", true, &MenuFlow::onMainPresets, this};
    mainItems_[1] = {"LOOP NAMES", true, &MenuFlow::onMainLoops, this};
    mainMenu_.setItems(mainItems_, 2);

    slotItems_[0] = {"SAVE LOOPS", true, &MenuFlow::onSlotSave, this};
    slotItems_[1] = {"RENAME", true, &MenuFlow::onSlotRename, this};
    slotItems_[2] = {"DELETE", true, &MenuFlow::onSlotDelete, this};
#if LOOPSWITCHER_ENABLE_MIDI
    slotItems_[3] = {"MIDI", true, &MenuFlow::onSlotMidi, this};
    for (uint8_t i = 0; i < kMidiMenuItems; ++i) midiContexts_[i] = {this, i};
#endif
    slotMenu_.setItems(slotItems_, kSlotMenuItems);

    for (uint8_t i = 0; i < kLabelCount; ++i) loopContexts_[i] = {this, i};
    for (uint8_t i = 0; i < kPresetCount; ++i) presetContexts_[i] = {this, i};
    refreshLoopList();
    refreshPresetList();
}

void MenuFlow::toast(const char* message) { stack_.showToast(message, lastInputMs_); }

void MenuFlow::tick(uint32_t nowMs) {
    if (stack_.depth() > 1 && nowMs - lastInputMs_ >= kMenuIdleMs) stack_.popToRoot();
}

void MenuFlow::refreshLoopList() {
    char name[kNameMax + 1];
    for (uint8_t i = 0; i < kLabelCount; ++i) {
        store_.loopLabel(i, name);
        rowText(loopText_[i], i, name);
        loopItems_[i] = {loopText_[i], true, &MenuFlow::onLoopSelected, &loopContexts_[i]};
    }
    loopMenu_.setItems(loopItems_, kLabelCount);
}

void MenuFlow::refreshPresetList() {
    char name[kNameMax + 1];
    for (uint8_t i = 0; i < kPresetCount; ++i) {
        if (store_.presetUsed(i)) {
            store_.presetName(i, name);
            rowText(presetText_[i], i, name);
        } else {
            rowText(presetText_[i], i, "EMPTY");
        }
        presetItems_[i] = {presetText_[i], true, &MenuFlow::onPresetSelected, &presetContexts_[i]};
    }
    presetMenu_.setItems(presetItems_, kPresetCount);
}

void MenuFlow::openMainMenu() {
    mainMenu_.resetHighlight();
    stack_.push(mainMenu_);
}

void MenuFlow::openPresetList() {
    refreshPresetList();
    presetMenu_.resetHighlight();
    stack_.push(presetMenu_);
}

void MenuFlow::openLoopList() {
    refreshLoopList();
    loopMenu_.resetHighlight();
    stack_.push(loopMenu_);
}

void MenuFlow::openSlotMenu(uint8_t slot) {
    editIndex_ = slot;
    // RENAME and DELETE only exist once a preset has been saved into the slot.
    slotMenu_.setItems(slotItems_, store_.presetUsed(slot) ? kSlotMenuItems : 1);
    slotMenu_.resetHighlight();
    stack_.push(slotMenu_);
}

#if LOOPSWITCHER_ENABLE_MIDI
void MenuFlow::openMidiMenu() {
    refreshMidiMenu();
    midiMenu_.resetHighlight();
    stack_.push(midiMenu_);
}

void MenuFlow::refreshMidiMenu() {
    const PresetMidi midi = store_.presetMidi(editIndex_);
    snprintf(midiText_[0], sizeof(midiText_[0]), "CHANNEL %u", static_cast<unsigned>(midi.channel + 1));
    snprintf(midiText_[1], sizeof(midiText_[1]), "BANK SELECT %s", midi.bankSelectEnabled ? "ON" : "OFF");
    snprintf(midiText_[2], sizeof(midiText_[2]), "BANK MSB %u", static_cast<unsigned>(midi.bankMsb));
    snprintf(midiText_[3], sizeof(midiText_[3]), "BANK LSB %u", static_cast<unsigned>(midi.bankLsb));
    snprintf(midiText_[4], sizeof(midiText_[4]), "PROGRAM %s", midi.programChangeEnabled ? "ON" : "OFF");
    snprintf(midiText_[5], sizeof(midiText_[5]), "PROGRAM %u", static_cast<unsigned>(midi.program + 1));
    snprintf(midiText_[6], sizeof(midiText_[6]), "EFFECT CC %s", midi.effectCcEnabled ? "ON" : "OFF");
    snprintf(midiText_[7], sizeof(midiText_[7]), "CC NUMBER %u", static_cast<unsigned>(midi.effectCc));
    snprintf(midiText_[8], sizeof(midiText_[8]), "CC VALUE %u", static_cast<unsigned>(midi.effectValue));
    for (uint8_t i = 0; i < kMidiMenuItems; ++i) {
        midiItems_[i] = {midiText_[i], true, &MenuFlow::onMidiSelectedCallback, &midiContexts_[i]};
    }
    midiMenu_.setItems(midiItems_, kMidiMenuItems);
}

void MenuFlow::startMidiEdit(MidiField field, const char* title, uint8_t value, uint8_t min, uint8_t max) {
    midiField_ = field;
    numberEntry_.reset(title, value, min, max);
    stack_.push(numberEntry_);
}

void MenuFlow::onMidiSelected(uint8_t item) {
    PresetMidi midi = store_.presetMidi(editIndex_);
    switch (item) {
        case 0: startMidiEdit(MidiField::Channel, "CHANNEL", static_cast<uint8_t>(midi.channel + 1), 1, 16); return;
        case 1: midi.bankSelectEnabled = !midi.bankSelectEnabled; break;
        case 2: startMidiEdit(MidiField::BankMsb, "BANK MSB", midi.bankMsb, 0, 127); return;
        case 3: startMidiEdit(MidiField::BankLsb, "BANK LSB", midi.bankLsb, 0, 127); return;
        case 4: midi.programChangeEnabled = !midi.programChangeEnabled; break;
        case 5: startMidiEdit(MidiField::Program, "PROGRAM", static_cast<uint8_t>(midi.program + 1), 1, 128); return;
        case 6: midi.effectCcEnabled = !midi.effectCcEnabled; break;
        case 7: startMidiEdit(MidiField::EffectCc, "CC NUMBER", midi.effectCc, 0, 127); return;
        case 8: startMidiEdit(MidiField::EffectValue, "CC VALUE", midi.effectValue, 0, 127); return;
        default: return;
    }

    if (store_.setPresetMidi(editIndex_, midi)) {
        refreshMidiMenu();
        toast("SAVED");
    } else {
        toast("NOT SAVED");
    }
}

void MenuFlow::onNumberDone(uint8_t value) {
    PresetMidi midi = store_.presetMidi(editIndex_);
    switch (midiField_) {
        case MidiField::Channel: midi.channel = static_cast<uint8_t>(value - 1); break;
        case MidiField::BankMsb: midi.bankMsb = value; break;
        case MidiField::BankLsb: midi.bankLsb = value; break;
        case MidiField::Program: midi.program = static_cast<uint8_t>(value - 1); break;
        case MidiField::EffectCc: midi.effectCc = value; break;
        case MidiField::EffectValue: midi.effectValue = value; break;
    }
    if (!store_.setPresetMidi(editIndex_, midi)) {
        toast("NOT SAVED");
        return;
    }
    stack_.pop();  // the number entry: back on the MIDI menu
    refreshMidiMenu();
    toast("SAVED");
}
#endif

void MenuFlow::startEdit(Edit kind, const char* initialText) {
    edit_ = kind;
    textEntry_.reset(initialText);
    stack_.push(textEntry_);
}

void MenuFlow::closeSlotMenuAfter(const char* message) {
    refreshPresetList();
    stack_.pop();  // the slot menu: back on the preset list
    toast(message);
}

void MenuFlow::onTextDone(const char* text) {
    switch (edit_) {
        case Edit::LoopLabel:
            if (!store_.setLoopLabel(editIndex_, text)) {
                toast("NOT SAVED");
                return;
            }
            refreshLoopList();
            stack_.pop();  // the text entry: back on the loop list
            toast("SAVED");
            break;
        case Edit::NewPreset:
            if (!store_.savePreset(editIndex_, text, controller_.loopMask())) {
                toast("NAME REQUIRED");
                return;
            }
            stack_.pop();  // the text entry
            closeSlotMenuAfter("SAVED");
            break;
        case Edit::RenamePreset:
            if (!store_.renamePreset(editIndex_, text)) {
                toast("NAME REQUIRED");
                return;
            }
            stack_.pop();
            closeSlotMenuAfter("RENAMED");
            break;
    }
}

void MenuFlow::onOverwriteChoice(bool yes) {
    stack_.pop();  // the confirmation: back on the slot menu
    if (!yes) return;
    if (!store_.setPresetMask(editIndex_, controller_.loopMask())) {
        toast("NOT SAVED");
        return;
    }
    closeSlotMenuAfter("SAVED");
}

void MenuFlow::onDeleteChoice(bool yes) {
    stack_.pop();
    if (!yes) return;
    if (!store_.deletePreset(editIndex_)) {
        toast("NOT DELETED");
        return;
    }
    closeSlotMenuAfter("DELETED");
}

void MenuFlow::onMainPresets(void* context) { static_cast<MenuFlow*>(context)->openPresetList(); }
void MenuFlow::onMainLoops(void* context) { static_cast<MenuFlow*>(context)->openLoopList(); }

void MenuFlow::onLoopSelected(void* context) {
    const ItemContext* item = static_cast<ItemContext*>(context);
    MenuFlow* flow = item->flow;
    flow->editIndex_ = item->index;

    char name[kNameMax + 1] = "";
    if (flow->store_.hasLoopLabel(item->index)) flow->store_.loopLabel(item->index, name);
    flow->startEdit(Edit::LoopLabel, name);
}

void MenuFlow::onPresetSelected(void* context) {
    const ItemContext* item = static_cast<ItemContext*>(context);
    item->flow->openSlotMenu(item->index);
}

void MenuFlow::onSlotSave(void* context) {
    MenuFlow* flow = static_cast<MenuFlow*>(context);
    if (flow->store_.presetUsed(flow->editIndex_)) {
        flow->overwriteConfirm_.reset();
        flow->stack_.push(flow->overwriteConfirm_);
    } else {
        flow->startEdit(Edit::NewPreset, "");
    }
}

void MenuFlow::onSlotRename(void* context) {
    MenuFlow* flow = static_cast<MenuFlow*>(context);
    char name[kNameMax + 1];
    flow->store_.presetName(flow->editIndex_, name);
    flow->startEdit(Edit::RenamePreset, name);
}

void MenuFlow::onSlotDelete(void* context) {
    MenuFlow* flow = static_cast<MenuFlow*>(context);
    flow->deleteConfirm_.reset();
    flow->stack_.push(flow->deleteConfirm_);
}

#if LOOPSWITCHER_ENABLE_MIDI
void MenuFlow::onSlotMidi(void* context) { static_cast<MenuFlow*>(context)->openMidiMenu(); }

void MenuFlow::onMidiSelectedCallback(void* context) {
    const MidiItemContext* item = static_cast<MidiItemContext*>(context);
    item->flow->onMidiSelected(item->index);
}

void MenuFlow::numberDone(void* context, uint8_t value) { static_cast<MenuFlow*>(context)->onNumberDone(value); }
#endif

void MenuFlow::textDone(void* context, const char* text) { static_cast<MenuFlow*>(context)->onTextDone(text); }
void MenuFlow::popScreen(void* context) { static_cast<MenuFlow*>(context)->stack_.pop(); }
void MenuFlow::overwriteChoice(void* context, bool yes) { static_cast<MenuFlow*>(context)->onOverwriteChoice(yes); }
void MenuFlow::deleteChoice(void* context, bool yes) { static_cast<MenuFlow*>(context)->onDeleteChoice(yes); }
