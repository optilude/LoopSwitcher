# Capability Map: LoopSwitcher Firmware

| Module id | Responsibility | Depends on |
|---|---|---|
| `loop-switching` | Translate loop on/off requests into relay states; provide loop-state operations for manual and preset behavior. | — |
| `preset-management` | Eight loop labels; eight fixed preset slots (name plus loop combination) with save, rename, modify and delete (deleting empties the slot); last loop state; persistence across power cycles. | `loop-switching` |
| `ui-framework` | OLED rendering (1.3" 128x64, large readable fonts), encoder/Back/Mode input events, menu system, text entry via the encoder. | — |
| `operating-modes` | Manual and preset modes; footswitch and LED handling (U2); display of the active preset or the last-touched loop's name; edit screens for labels and presets built on the other modules. | `loop-switching`, `preset-management`, `ui-framework` |

Build order: `loop-switching` → `preset-management` and `ui-framework` (parallel) → `operating-modes`.

Revision history: `control-ui` was split into `ui-framework` and `operating-modes`, and `preset-management` was widened to include loop labels (approved 2026-10-01).
