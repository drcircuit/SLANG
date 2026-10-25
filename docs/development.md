# Native language development

The current native milestone is a C++20 frontend, AST interpreter, interactive CLI, and SDL graphics library. It validates names/control flow and enforces types during execution. Full static type inference and bytecode compilation remain future work. SDL 3.4.18 is a pinned static source dependency; Unicode identifier tables are checked into the repository. Node.js is optional and is used only to compare behavior against the original syntax prototype.

CMake downloads SDL from its official release and verifies SHA-256 `9cd42377704398796071b8597cd7e21da254a43bad98c4199739647adc13fa6f`. For an offline build, place the verified `SDL3-3.4.18.zip` in `build/downloads/` before configuring, or use CMake's `FETCHCONTENT_SOURCE_DIR_SDL3` override with the matching source tree. The finished package needs no downloads. SDL and embedded third-party notices are installed under `licenses/SDL3`.

## Windows

Developer prerequisites: Visual Studio C++ tools and Windows SDK, CMake 3.24 or newer, and Ninja. Visual Studio's bundled CMake/Ninja can be used. These requirements apply to rebuilding SLANG, not to running a packaged executable.

From the repository root:

```powershell
.\scripts\build-native.ps1
.\build\native-ninja\slang.exe check .\examples\english.slang .\examples\norsk.slang
```

The helper discovers Visual Studio, imports the compiler environment for its process, and uses Ninja. This also avoids MSBuild dependency-tracker failures in restricted environments. It runs the native tests after building.

To produce a portable interpreter ZIP and compare native parsing with the JavaScript reference:

```powershell
.\scripts\build-native.ps1 -Package -VerifyParity
```

`-VerifyParity` requires Node.js 22 or newer; omit it for a completely native build/test/package workflow. The ZIP is placed in `dist/`. `-Configuration Debug` enables a debug build. Use a separate `-BuildDirectory` when keeping debug and release builds side by side.

The Windows target links SDL and the Visual C++ runtime statically. It does not require an SDL DLL, Node.js, .NET, Python, or the Visual C++ redistributable. Normal operating-system graphics drivers are still used.

## macOS and Linux

Use a C++20 compiler and CMake 3.24 or newer:

```sh
cmake -S . -B build/native -DCMAKE_BUILD_TYPE=Release
cmake --build build/native --parallel
ctest --test-dir build/native --output-on-failure
./build/native/slang check examples/english.slang examples/norsk.slang
```

For a multi-configuration generator, add `--config Release` to the build and `-C Release` to CTest, and use the configuration-specific executable path.

Package with:

```sh
cd build/native
cpack -C Release -B ../../dist
```

These platforms are build targets, but have not been compiled or tested in the current Windows environment. Their standard-library linkage, OS compatibility floor, and release packaging require validation before distribution.

## Tests

CTest runs frontend, runtime, module, and graphics test executables, syntax and execution of examples, and CLI integration checks. Graphics tests use SDL's dummy video/software renderer for deterministic pixel checks; run `slang_graphics_tests <output-directory>` without those environment settings to exercise desktop windows. The suite covers:

- English/Norwegian normalization, Unicode identifiers, escapes, and invalid UTF-8.
- Exact numeric literal spelling, including values larger than machine integers.
- Calls, comparisons, precedence, ambiguity diagnostics, and multiline expressions.
- Lists, objects, field access, computed indices, updates, and anonymous functions.
- Control flow, block terminators, and source locations.
- CLI exit codes, standard input, JSON diagnostics, and paths with spaces and Scandinavian letters.
- Deeply nested input failing with a diagnostic rather than overflowing the stack.
- Exact arithmetic, rounding, large integers, and bounded arithmetic checked against native integer results.
- Closures, recursion limits, overloads, strong binding types, aliases, cycles, garbage collection, and persistent sessions.
- File execution, console input, multiline REPL submissions, error recovery, and execution limits.
- Module privacy, exported nominal types, source-relative imports, aliases, shared initialization, cycles, failed initialization, cached closures, and reset.
- Implicit canvas creation, retained pixel rendering, resizing, cached PNG sprites, keyboard snapshots, close events, loop/callback execution, callback source errors, graphics argument checks, frame budgets, cancellation, and console input with an open canvas.

With Node available, the JavaScript tests can run against the native executable. Every successful native parse in that suite is compared structurally with the reference AST. `-VerifyParity` also checks 600 deterministic large-number/decimal arithmetic results against an independent Node BigInt oracle, including floor, ceil, powers, and square roots. Run that oracle directly with `node scripts/verify-arithmetic.mjs <slang executable>`. CTest includes standard-library integration tests and the initial classroom library examples; file-writing tests use the build directory.

`npm run examples:test` exercises the paired [English and Norwegian showcases](../examples/README.md), including input validation, file persistence, matching chart pixels, keyboard-controlled animation and collecting a coin. It uses SDL's dummy/software backend, copies examples into a unique `build/showcase-tests/` directory, and saves chart previews there. Set `SLANG_NATIVE_EXE` to test a different native build. GitHub Actions runs this suite on each platform before publishing package artifacts.

```powershell
$env:SLANG_NATIVE_EXE = (Resolve-Path .\build\native-ninja\slang.exe).Path
node --test test/language.test.js
```

On macOS/Linux:

```sh
SLANG_NATIVE_EXE="$PWD/build/native/slang" node --test test/language.test.js
```

Without this environment variable, `npm test` exercises the original JavaScript parser only. The explicit test path avoids discovering upstream SDL/Emscripten fixtures under build directories. Neither JavaScript nor its runtime is included in the native package.

## Electron IDE

Developer prerequisites are Node.js 22.12+ and the native build above. Students use the packaged application without installing either toolchain.

```powershell
npm ci
# If npm disables dependency install scripts, explicitly install Electron:
node node_modules/electron/install.js
npm run ide
npm run ide:test
npm run ide:smoke
npm run ide:themes
npm run ide:package
```

`ide:build` bundles Monaco and its worker with esbuild. `ide:smoke` launches the actual Electron application with Playwright and checks run/debug/step, completion, diagnostics, input, canvas, pause/resume/stop, save/cancel, and renderer isolation. Screenshots are written under `build/ide-smoke`. The protocol tests use SDL's dummy video driver. `SLANG_BINARY` can select another native build. `SLANG_IDE_PROFILE` selects an isolated application profile for tests.

`ide:themes` exercises every light/dark palette in Electron, checks that canvas pixels retain their original colors, verifies the compact toolbar and keyboard selection, and restarts the app to verify persistence. Theme screenshots are in `build/ide-themes`. `ide:test` also checks text/syntax contrast ratios and invalid saved preferences. Theme definitions and Monaco colors share `ide/themes.mjs`; shell styling uses the same CSS color variables.

`ide:package` produces an archive for the current host platform/architecture under `dist`: ZIP on Windows/macOS and tar.gz on Linux. It includes Electron, the native binary, examples, documentation, and licenses. See [release guide](releases.md) for GitHub Actions and platform status. It refuses to overwrite an existing package. Build the native runner and renderer first. Set `SLANG_TEST_PACKAGED=1` to test the executable recorded in `build/ide-package.json`, or set `SLANG_IDE_EXECUTABLE` to a specific packaged executable. `SLANG_IDE_MINIMAL_PATH=1` removes development runtimes from the child application's PATH.

`ide/main.cjs` owns file access and runner processes; `ide/preload.cjs` exposes only named operations to the sandboxed renderer. `ide/renderer.js` manages Monaco and the panels; `ide/language.mjs` contains tolerant completion/signature metadata. `native/src/ide.cpp` implements the framed runner protocol; `host.hpp` keeps the evaluator and SDL backend independent of Electron. See [IDE behavior and current limits](ide.md).

## Code organization

| Location | Purpose |
| --- | --- |
| `native/include/slang/source.hpp` | Source positions, spans, structured diagnostics |
| `native/include/slang/ast.hpp` | Immutable syntax nodes with typed C++ variant payloads |
| `native/src/lexer.cpp` | UTF-8 validation, tokens, keyword normalization |
| `native/src/parser.cpp` | Statements and expression alternatives with bounded search |
| `native/src/ast.cpp` | Stable JSON serialization without source locations |
| `native/src/number.cpp` | Arbitrary-size integer and scaled-decimal arithmetic |
| `native/src/runtime.cpp` | Name/control-flow validation, interpreter, modules, visibility, closures, types, builtins, heap tracing |
| `native/src/main.cpp` | Native CLI, Unicode paths on Windows, diagnostic rendering |
| `native/tests/` | Native behavior and CLI tests |
| `src/` and `test/` | Original syntax reference and optional conformance tests |

Syntax nodes share immutable subtrees during alternative parsing. Runtime objects, functions, and scopes use a separately owned heap, traced from session globals, modules, and explicit evaluator roots during execution. Runtime graph edges are non-owning, so unreachable cycles can be collected. Closures retain their ASTs and defining source for later diagnostics. Root guards protect active environments, pending arguments, temporary lists, assignment owners, and iteration snapshots while nested calls collect. New evaluator paths that suspend across another call must register any live temporary heap values before that call.

`slang check --json` emits one JSON object per input file. Each result identifies the `syntax` phase. Diagnostic offsets are UTF-8 byte offsets; line and column numbers are one-based, with columns counted as Unicode scalar values. An LSP adapter must convert positions to the encoding negotiated with its client. Human-readable caret rendering expands tabs; display-width alignment for combining or wide characters is not yet implemented.

Windows interpreter executables reserve an 8 MiB stack so the AST evaluator's explicit limits can fire safely. Embedders of `slang_core` on Windows must provide the same stack reservation. Execution has a 32-call function limit and a shared 128-frame evaluation limit; bytecode execution should remove reliance on recursive native evaluator frames.

## Updating Unicode tables

`native/src/unicode_ranges.inc` is generated from Unicode letter, mark, and number categories. Its header records the Unicode version used. Maintainers with Node can regenerate it using:

```sh
node scripts/generate-unicode.mjs
```

Regeneration is not part of native builds. Review table changes when upgrading the generation runtime.

## Current limits

The parser deliberately retains the reference's conservative handling of angle-bracket ambiguity. For example, `check<a > b>` parses, while `check<a < b>` requires grouping. No type-based disambiguation occurs yet. Search and nesting bounds protect the frontend; they are not final language limits.

`check` still checks syntax only. `run` and `repl` additionally validate names and control-flow placement before execution and enforce types while evaluating. See [runtime rules](runtime.md) for the initial numerical, collection, overload, and absence policies, execution budgets, and the 32-call recursion limit. Preview archives must identify these as interpreter features, not a completed static compiler or IDE.
