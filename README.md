# SLANG

A programming language for beginners and classroom teaching, designed for comfortable typing on Scandinavian keyboards.

**Licensing:** the language, IDE and original tooling use [MIT](LICENSE). Original [graphics examples and assets](examples/graphics/README.md) use CC0 1.0 Universal. Dependencies retain their [own notices](THIRD-PARTY-NOTICES.md).

**Builds and releases:** [GitHub Actions and platform packaging](docs/releases.md) cover Windows x64, Linux x64, and Intel/Apple Silicon macOS. Branches and PRs build/test packages; matching `v*` tags create draft releases after all targets pass.

**Version 0.6 adds the Electron IDE:** a Monaco editor, English/Norwegian syntax highlighting, ordinary non-AI code completion, a file browser, console and SDL canvas panels, and working Run, Stop, Pause, and Debug controls. Statement breakpoints, step-into, and variable summaries are included. The native C++20 interpreter supplies arbitrary-size integers, decimal arithmetic, closures, libraries, mutable lists and objects, and SDL graphics. The Windows IDE package includes its runtimes and works after extraction.

This is an interpreter and IDE preview. Name and control-flow validation happen before execution; strong type checks happen during execution. Full static type inference, a bytecode compiler, advanced debugging, structured file formats, and IoT libraries remain future work.

**IDE 0.6.1 adds appearance controls:** light and dark modes, with teal, pink, purple, blue, green, yellow, orange, and high contrast themes. Choose Mode and Theme at the top right; the selection is remembered between sessions.

## Start programming

**Classroom materials:** The quickstart, core courses and topic workbooks are maintained separately from this repository. Their editable sources, student examples, PDFs and distribution packages are not included in the language or IDE source distribution.

**Language settings:** choose Norsk or English in the IDE for its interface and code suggestions. Standard libraries have Norwegian aliases such as `vindu.knapp`. A first-line `#no` or `#en` selects a file's diagnostic language; the REPL accepts both vocabularies together. See [localization](docs/localization.md).

**Try the paired classroom examples:** [English guide](examples/README.md) · [Norsk veiledning](examples/LESMEG.md). Eleven lessons cover everyday programs, exact numbers, objects, closures, libraries, files, charts, animation, and a small coin-collecting game.

Extract the **SLANG-IDE** ZIP and double-click **SLANG.exe**. Press the green **Run** button or F5. See the [IDE guide](docs/ide.md) for editing, completion, debugging, and canvas input. **SLANG-console.cmd** opens the separate persistent REPL; `slang.cmd` runs CLI commands from the IDE package.

The smaller native-only ZIP remains available: double-click **SLANG.cmd** to open its console, or open a terminal in that extracted folder:

```powershell
.\bin\slang.exe repl
.\bin\slang.exe run .\examples\first-program.slang
.\bin\slang.exe run .\examples\gjett.slang
.\bin\slang.exe run .\examples\modules\main.slang
.\bin\slang.exe run .\examples\graphics\drawing.slang
.\bin\slang.exe run .\examples\graphics\loop.slang
.\bin\slang.exe run .\examples\graphics\callback.slang
```

Try this in the console:

```text
>>> var price = 19.95
>>> price * 3
59.85
>>> 0.1 + 0.2
0.3
>>> 999999999999999999999999999999 + 1
1000000000000000000000000000000
```

Enter blocks across multiple lines and close them with a period. `:help` shows console help, `:cancel` discards unfinished input, `:reset` clears the session, and `:quit` exits. `type<value>` displays a value's type. Ctrl+C interrupts execution. A configurable execution limit catches endless loops.

## Draw and animate

```text
use graphics as g
g.fill<"blue">
g.circle<200, 150, 40>
```

The first drawing creates a canvas. It stays open until closed, while `print` continues to use the console. Animate with an ordinary `while g.open<>:` loop and `g.present<>`, or supply a function to `g.run<Frame>`. See [graphics examples and reference](docs/graphics.md). Run `examples/graphics/sprite-benchmark.slang` to measure a 120-sprite workload on your machine.

## English and Norwegian

```text
func Add <a, b>:
    ret a + b
.
print<Add<2, 3>>
```

```text
funk Add <a, b>:
    retur a + b
.
print<Add<2, 3>>
```

`eller` means `or` between expressions and `else` at a branch boundary. `eller hvis` means `else if`; `stopp` means `break`. Indentation never changes program meaning. Both keyword languages produce the same AST and use the same runtime. Names, library calls, and text retain their spelling; diagnostics are currently English.

## Libraries

```text
lib maths:
    public func Square <val>:
        ret val * val
    .
.
```

Save that as `maths.slang`, then import it from another file:

```text
use "maths.slang" as maths
use console
console.print<maths.Square<5>>
```

Unmarked declarations inside `lib` are private. Imports resolve relative to the importing file and share one module instance per session. Exported object types support qualified annotations such as `var player as players.Player = ...`. Bundled `console`, `numbers`, `strings`, `lists`, `random`, `time`, `files`, `graphics`, and `window` libraries work without external files. See the [student standard library](docs/standard-library.md), [window controls](docs/windows.md), [module rules](docs/modules.md), and examples in `examples/standard-library/`, `examples/windows/`, and `examples/modules/`.

## Commands

| Command | Behavior |
| --- | --- |
| `slang` or `slang repl` | Persistent interactive console |
| `slang run file.slang` | Validate names/control flow and execute with runtime type checks |
| `slang run --max-steps 5000000 file.slang` | Set execution budget; `0` means unlimited |
| `slang check [--json] files...` | Syntax checking only |
| `slang ast file.slang` | Inspect the syntax tree |

Use `-` instead of a filename to read source from standard input. When source uses stdin, `input<>` sees EOF; use a source file for interactive programs. Exit codes are 0 for success, 1 for source/runtime/I/O errors, and 2 for invalid command usage. The REPL reports errors and keeps the session open.

## Implemented behavior

- Arbitrary-size signed integers; exact decimal addition, subtraction, and multiplication. `/` produces a decimal rounded to 28 fractional places, ties to even. `round<value, places>` and `divide<a, b, places>` provide explicit rounding.
- Explicit declarations with optional annotations, stable inferred binding types, explicit conversions, and `none`.
- Functions, typed overloads, recursion, anonymous functions, and closures with mutable captures.
- Mutable lists, one-based indexing, named/positional objects, fixed typed fields, and shared references.
- Conditions, while/repeat/range/collection loops, returns, break/continue, and prefix/postfix updates.
- `print`, `input`, `length`, `append`, `remove`, `type`, `round`, `divide`, `assert`, and `error`.
- Libraries, public exports, private helpers/state, source imports, bundled namespaces, cycle diagnostics, and module caching/reset.

See [runtime rules and examples](docs/runtime.md) for initial choices about rounding, absence, collection typing, equality, overloads, and execution limits. These rules are documented implementation choices for review, not a frozen specification.

Some angle expressions remain ambiguous: `check<a > b>` works, while `check<a < b>` requires `check<(a < b)>`. The parser reports ambiguity rather than guessing.

## Build and test

With Visual Studio C++ tools, CMake, and Ninja installed on a development machine:

```powershell
.\scripts\build-native.ps1 -Package -VerifyParity
.\build\native-ninja\slang.exe run .\examples\first-program.slang
```

`-VerifyParity` optionally uses Node to compare native parsing with the JavaScript reference; omit it for a fully native workflow. CMake fetches the pinned SDL 3.4.18 source archive and links it statically. Developer tools and downloads are not needed to use the packaged executable. SDL and bundled component notices are shipped under `licenses/`. See [development instructions](docs/development.md).

Windows x64 has been built and tested locally. macOS/Linux are CMake targets but have not yet been verified; complete platform releases remain on the roadmap.

See the [language design](docs/language-spec.md) and [product roadmap](docs/roadmap.md) for the planned compiler, IDE, standard libraries, and complete offline distribution.
