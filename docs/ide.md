# SLANG IDE 0.6.2

The Electron IDE combines a Monaco code editor, file browser, console, SDL canvas, and a basic statement debugger. Everything it uses ships in the Windows ZIP. There is no account, AI completion, network service, or first-launch download.

Extract the complete package for your platform and open **SLANG.exe** on Windows, **SLANG.app** on macOS, or **SLANG** on Linux. The welcome program is ready to run. Open the graphics folder in the file browser for drawing, loop, callback, and sprite examples. See [builds and releases](releases.md) for platform status and signing limitations.

## Language

Choose **Norsk** or **English** in the toolbar. The preference is saved between sessions and changes the app controls, dialogs, help and suggested standard-library names. Norwegian completion offers `vindu.knapp`, for example, while English completion offers `window.button`. Both languages remain valid source, and your code and custom names are preserved when switching. Some Monaco editor messages and external system details remain English.

Put `#no` or `#en` on the first line of a source file to choose its diagnostic language independently of the interface. Without a directive, the IDE setting is used. Program output is unchanged. See [language aliases and directives](localization.md).

Colour suggestions also follow this setting. Type the opening quote in `grafikk.fyll<"` or `grafikk.bakgrunn<"` to see names such as `hvit`, `lilla` and `gul` with **Norsk** selected; **English** offers `white`, `purple` and `yellow`. Ctrl+Space opens the suggestions again inside the colour name. Both sets of names remain valid regardless of the setting.

## Appearance

Use **Mode** and **Theme** at the top right to choose Light or Dark, with Teal, Pink, Purple, Blue, Green, Yellow, Orange, or High contrast. Every theme works in both modes. The editor, syntax colors, completion menus, console, and debugger change together; colors drawn by a program stay as written. High contrast adds stronger boundaries, selection outlines, and focus indicators.

The selection is saved locally and restored when the IDE reopens. On first launch, light/dark follows the operating system preference. Both dropdowns support keyboard navigation. IDE and interpreter versions are synchronized starting with 0.6.2.

## Edit and run

| Action | Shortcut | Behavior |
| --- | --- | --- |
| Run / Continue | F5 | Run the current editor text, or continue a paused program |
| Stop | Shift+F5 | Interrupt execution, including graphics loops and console input |
| Pause / Resume | F6 | Cooperatively pause or resume the native runner |
| Debug | Ctrl+F5 | Start paused before the first executable statement |
| Step | F11 | Advance to the next statement, entering called functions |
| New / Open | Ctrl+N / Ctrl+O | Create a buffer or choose a source file |
| Save / Save as | Ctrl+S / Ctrl+Shift+S | Save the active buffer |
| Close tab | Ctrl+W | Close after checking for unsaved changes |
| Suggestions | Ctrl+Space | Open deterministic code completion |
| Find | Ctrl+F | Search the editor |

On macOS, the usual Command modifier replaces Control for file shortcuts.

Open folder changes the file browser's root. Expand a directory with its arrow and click a file to open it. Open buffers have tabs; a dot marks unsaved edits. Save, Discard, and Cancel are available when closing modified tabs or the application. Drag the dividers to resize the file browser, canvas, and console; focused dividers also accept arrow keys.

Run takes a temporary snapshot of the current buffer, without overwriting the original file. Relative imports, images, and `files` library paths resolve from that file's directory; unsaved buffers use the open folder. **Save imported modules before running:** they are loaded from disk. Editing is disabled while a program runs so source locations continue to match the running snapshot. Stop to edit again.

The console displays `print` output and accepts responses to `input`. EOF ends input. It retains a bounded amount of output so printing in a loop cannot grow the UI indefinitely. The separate **SLANG-console.cmd** launcher opens the persistent REPL; the IDE's console is not yet a REPL.

## Editor assistance

English and Norwegian keywords have the same highlighting and semantics. Completion suggests keywords, builtins, visible local variables and function parameters, function signatures, object fields, bundled library names after `use`, bundled library members, and explicitly public declarations from imported source libraries. All [student standard libraries](standard-library.md) have member completion and parameter hints. Suggestions and argument help are computed locally. The native parser supplies syntax diagnostics while typing; runtime errors link to their source and underline the affected expression.

This first completion service is a tolerant declaration scanner, not a full type checker or LSP server. It handles the normal one-statement-per-line classroom style, including incomplete expressions. Complex reassignment, inferred return types, overload selection, and unusual multiline declarations can require manual typing. Angle brackets are also comparison operators, so signature hints in ambiguous expressions are best-effort. Suggestions do not change or validate program semantics. Full static inference remains future work.

## Canvas

The canvas also hosts [window components](windows.md). Open `examples/windows/english/club.slang` or its `norsk` counterpart to try a form with all seven controls. Click a control to interact, use Tab/Shift+Tab to navigate, and type normally, including Norwegian characters. Window control properties such as `.caption`, `.value`, `.selected`, and `.enabled` appear in completion for directly constructed controls.

The panel shows pixels from the **same native SDL renderer** used by the CLI. No separate JavaScript graphics implementation is involved. Drawing automatically creates the canvas. A static program finishes and leaves its last image visible. Ordinary `while g.open<>:` loops and `g.run<Frame>` callbacks both work.

Click the canvas to send keyboard input to the running program. Clicking elsewhere or changing windows releases held keys. The Close button requests the same canvas closure as closing a CLI window. Stop terminates the entire program. The preview fits the available tile while preserving the drawing's aspect ratio.

The native process permits one outstanding frame and waits for an acknowledgement before publishing another. Slow UI rendering drops intermediate previews instead of accumulating unbounded frames. Frame copies and Electron IPC add overhead compared with the CLI. Use the included sprite benchmark on the target classroom machines; 60 FPS is a target rather than a guarantee.

## Debugger

Click the gutter to set or remove a line breakpoint. Use an executable statement: comments, declarations, and closing periods do not themselves execute. Debug initially stops before the first statement. Run continues to the next breakpoint; Step enters function calls and stops at the next statement. The yellow line indicates the current location, and the variables panel shows visible names, their types, and bounded value summaries.

Pausing during native input or graphics servicing can stop between statement checkpoints. At that point the last source location is shown and variable inspection may be unavailable; Step reaches the next inspectable statement. The debugger currently has no step-over, step-out, conditional breakpoints, watch expressions, full call-stack view, or value editing. Breakpoints belong to the current IDE session.

## Implementation and distribution

The renderer runs with Chromium sandboxing, context isolation, no Node integration, a restrictive content policy, and a narrow preload API. The main process owns file dialogs and workspace access. SLANG programs execute in a separate native process with the user's normal filesystem permissions; the IDE is not a sandbox for untrusted SLANG programs.

The internal `slang ide <source> --snapshot <temporary-file> [--debug]` command accepts newline-delimited control messages on stdin. Stdout contains a 4-byte little-endian JSON length, UTF-8 JSON, and optional RGBA frame bytes. Console output is framed as data. Pause, resume, step, breakpoints, input, keys, close, and stop are cooperative engine controls. Stop has a forced-process-exit fallback. This protocol is internal and versioned independently from a future DAP interface.

The package includes Electron, Monaco, native SLANG, statically linked SDL, examples, documentation, and dependency notices. User data and temporary snapshots use Electron's user-data directory, so the installed application can be read-only. Cross-platform packaging and CI are configured; Windows is locally tested, and macOS/Linux await their first CI run. Publisher signing, notarization, installers, and clean-machine qualification remain release work. See [release guide](releases.md).
