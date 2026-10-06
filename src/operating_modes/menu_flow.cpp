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

#if LOOPSWITCHER_ENABLE_MIDI
bool parseMidiValue(const char* text, uint8_t max, uint8_t& value) {
    if (text[0] == '\0') return false;

    uint16_t parsed = 0;
    for (uint8_t i = 0; text[i] != '\0'; ++i) {
        if (text[i] < '0' || text[i] > '9') return false;
        parsed = static_cast<uint16_t>(parsed * 10 + text[i] - '0');
        if (parsed > max) return false;
    }
    value = static_cast<uint8_t>(parsed);
    return true;
}
#endif

}  // namespace

MenuFlow::MenuFlow(ScreenStack& stack, PresetStore& store, PerformanceController& controller)
    : stack_(stack),
      store_(store),
      controller_(controller),
      textEntry_(&MenuFlow::textDone, &MenuFlow::textCancel, this),
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

void MenuFlow::startMidiEdit(Edit kind, uint8_t value) {
    char initial[4];
    snprintf(initial, sizeof(initial), "%u", static_cast<unsigned>(value));
    startEdit(kind, initial);
}

void MenuFlow::onMidiSelected(uint8_t item) {
    PresetMidi midi = store_.presetMidi(editIndex_);
    switch (item) {
        case 0: startMidiEdit(Edit::MidiChannel, static_cast<uint8_t>(midi.channel + 1)); return;
        case 1: midi.bankSelectEnabled = !midi.bankSelectEnabled; break;
        case 2: startMidiEdit(Edit::MidiBankMsb, midi.bankMsb); return;
        case 3: startMidiEdit(Edit::MidiBankLsb, midi.bankLsb); return;
        case 4: midi.programChangeEnabled = !midi.programChangeEnabled; break;
        case 5: startMidiEdit(Edit::MidiProgram, static_cast<uint8_t>(midi.program + 1)); return;
        case 6: midi.effectCcEnabled = !midi.effectCcEnabled; break;
        case 7: startMidiEdit(Edit::MidiEffectCc, midi.effectCc); return;
        case 8: startMidiEdit(Edit::MidiEffectValue, midi.effectValue); return;
        default: return;
    }

    if (store_.setPresetMidi(editIndex_, midi)) {
        refreshMidiMenu();
        toast("SAVED");
    } else {
        toast("NOT SAVED");
    }
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
#if LOOPSWITCHER_ENABLE_MIDI
        case Edit::MidiChannel:
        case Edit::MidiBankMsb:
        case Edit::MidiBankLsb:
        case Edit::MidiProgram:
        case Edit::MidiEffectCc:
        case Edit::MidiEffectValue: {
            const uint8_t max = edit_ == Edit::MidiChannel ? 16 : edit_ == Edit::MidiProgram ? 128 : 127;
            uint8_t value = 0;
            if (!parseMidiValue(text, max, value) ||
                ((edit_ == Edit::MidiChannel || edit_ == Edit::MidiProgram) && value == 0)) {
                toast("INVALID VALUE");
                return;
            }

            PresetMidi midi = store_.presetMidi(editIndex_);
            switch (edit_) {
                case Edit::MidiChannel: midi.channel = static_cast<uint8_t>(value - 1); break;
                case Edit::MidiBankMsb: midi.bankMsb = value; break;
                case Edit::MidiBankLsb: midi.bankLsb = value; break;
                case Edit::MidiProgram: midi.program = static_cast<uint8_t>(value - 1); break;
                case Edit::MidiEffectCc: midi.effectCc = value; break;
                case Edit::MidiEffectValue: midi.effectValue = value; break;
                default: return;
            }
            if (!store_.setPresetMidi(editIndex_, midi)) {
                toast("NOT SAVED");
                return;
            }
            stack_.pop();
            refreshMidiMenu();
            toast("SAVED");
            break;
        }
#endif
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
#endif

void MenuFlow::textDone(void* context, const char* text) { static_cast<MenuFlow*>(context)->onTextDone(text); }
void MenuFlow::textCancel(void* context) { static_cast<MenuFlow*>(context)->stack_.pop(); }
void MenuFlow::overwriteChoice(void* context, bool yes) { static_cast<MenuFlow*>(context)->onOverwriteChoice(yes); }
void MenuFlow::deleteChoice(void* context, bool yes) { static_cast<MenuFlow*>(context)->onDeleteChoice(yes); }
