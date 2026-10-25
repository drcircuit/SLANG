# Window components

`use window as ui` provides a small form library for student applications. The same SDL-drawn controls work in a standalone native window and in the IDE canvas. No external font, toolkit installation, or extra runtime is needed: the interpreter embeds Noto Sans and its font renderer.

```text
use window as ui

ui.create<"Hello", 480, 360>
var name = ui.input<"Your name", "Åse">
var message = ui.label<"Ready">

func Greet <>:
    message.caption = "Hello, " + name.value + "!"
.

ui.button<"Greet", Greet>
ui.button<"Close", ui.close>
ui.run<>
```

Create the window, add controls in reading order, then call `run`. Controls automatically fill the available width and stack vertically. The page scrolls when necessary. A button receives a function value, such as `Greet`, without `<>`; the event loop calls `Greet<>` when activated. Callbacks have no arguments and can capture local variables.

## Controls and properties

| Constructor | Properties |
| --- | --- |
| `label<caption>` | `.caption` changes the displayed label |
| `input<caption, initial?>` | `.value` contains editable single-line text; initial defaults to `""` |
| `button<caption, callback>` | Invokes `callback<>` on activation |
| `checklist<caption, items>` | `.selected` is a mutable `list of text` containing checked items |
| `radio_group<caption, items>` | `.value` contains the single chosen item |
| `listbox<caption, items>` | `.value` contains the single chosen item in a visible list |
| `combobox<caption, items>` | `.value` contains the single chosen item in a drop-down list |

Every constructor returns an object with `.caption` (`text`) and `.enabled` (`bool`). The selection controls also have `.items` (`list of text`). Norwegian programs can use `bruk vindu`, `vindu.knapp`, `vindu.tekstfelt`, and the properties `.tekst`, `.aktiv`, `.verdi`, `.elementer`, and `.valgte`. These are aliases for the same controls and values; see the complete [name table](localization.md). Names following a member-access dot may be keywords, so `.tekst` is valid even though `tekst` also names a type. Your own object fields are never renamed automatically.

```text
var interests = ui.checklist<"Interests", list<"Code", "Art", "Music">>
var level = ui.radio_group<"Level", list<"Beginner", "Explorer">>
interests.selected = list<"Code", "Music">
level.value = "Explorer"
level.enabled = false
```

Checklists start with no selections. Radio groups, list boxes and combo boxes initially select their first item; empty lists have `none` as their value. You can explicitly set `.value = none`. Items must be unique text strings. Setting a selected value that is not in `.items` is an error; when changing the items, update the selected value in the same callback if needed. Checklist selections are reported in item order. The constructor copies the supplied list so later changes to the original list do not silently change the control; modify the returned control's `.items` instead.

Properties are synchronized before interaction and before drawing. Button callbacks see current input and selection values. Changes to captions, values, selections, items and enabled state appear on the next drawing pass. Disabled controls cannot be activated or reached with Tab. All controls must be created before `run`; adding controls and nested event loops inside callbacks are errors. Callback errors retain their original source location and terminate execution just like ordinary function errors.

## Window lifecycle

- `create<title, width?, height?>` opens a new form and clears the previous form's controls. Defaults: 480 × 640. Supported sizes: width 320–1600 and height 240–1600.
- `run<>` processes events until the window closes. It keeps interpreter Stop, Pause, stepping and callback breakpoints available. Long-running callback code can still block interaction, just as an ordinary long-running function can.
- `close<>` closes the form and ends its event loop. It can be used directly as a button callback. The native close button and the IDE's canvas Close button also end the loop.
- One window/canvas is supported per interpreter session. Close a form before drawing with `graphics`; create another form or canvas for the next activity. Multiple windows, mixing a drawing area inside a form, menus, dialogs, custom layouts and change-event callbacks are future extensions.

## Keyboard and mouse

In the IDE, **click the canvas before typing**. The text input bridge uses actual text input events, so Scandinavian letters, dead keys, composition commits and pasted text reach the native controls without guessing a keyboard layout.

| Action | Input |
| --- | --- |
| Focus next/previous control | Tab / Shift+Tab |
| Choose an option | Click a row, or use Up/Down/Home/End |
| Toggle a checkbox | Click it, or focus its row with arrows and press Space |
| Activate a button | Click, Enter, or Space |
| Open/close a combo box | Click, Enter, or Space |
| Scroll a long form | Mouse wheel; tabbing also reveals the focused control |
| Reach more than four list choices | Arrow keys scroll the focused row into view; wheel scrolls an open combo menu |
| Edit input | Left/Right/Home/End, Backspace/Delete, Ctrl+A or Command+A to select all, paste |
| Close a menu/form | Escape closes an open combo menu; otherwise it closes the form |

This first version has single-line inputs and captions. Clicking an input focuses it with the caret at the end. Mouse text selection, partial Shift selections, copy/cut, undo, password fields and multiline editing are not implemented yet. Backspace and movement operate on Unicode scalar values, not grapheme clusters. The embedded font covers Nordic letters and common Latin/Greek/Cyrillic text; complex-script shaping, bidirectional layout and emoji fonts are not included. These are custom-drawn controls; native screen-reader/OS accessibility integration remains future work.

Limits for this version: 100 controls per form, 100 choices per selection control, and 4096 UTF-8 bytes per caption/input/choice. Input beyond the text limit is ignored; invalid programmatic property values raise a diagnostic. Window colors currently use a fixed light palette independent of the IDE theme.

## Examples

- [English greeting](../examples/windows/english/hello.slang) / [norsk hilsen](../examples/windows/norsk/hello.slang)
- [English school club form](../examples/windows/english/club.slang) / [norsk skoleklubb](../examples/windows/norsk/club.slang): all seven components together

Run an example in the IDE, or use `slang run examples/windows/english/club.slang`. The examples use MIT; the bundled font and font renderer retain their respective third-party licenses.
