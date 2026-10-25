# Learn SLANG by running small programs

[Norsk veiledning](LESMEG.md)

These **11 paired lessons** have English and Norwegian versions. Each pair teaches the same ideas; the Norwegian versions also translate comments, variable names, prompts, and output. Matching filenames make it easy to compare them.

In the IDE, expand **showcase → english** or **showcase → norsk** in the file browser, open a numbered file, and press **Run** (F5). Enter responses in the console input box and press Enter. **Stop** ends a running program; **Debug** and **Step** let you watch variables change. Begin with the first four lessons, then try graphics or continue with functions and libraries.

From a terminal, use `slang run path/to/example.slang`. For a source build on Windows:

```powershell
.\build\native-ninja\slang.exe run .\examples\showcase\english\01-hello.slang
.\build\native-ninja\slang.exe run .\examples\graphics\english\03-coins.slang
```

## Console lessons

| Lesson | English | Norwegian | What to explore |
| --- | --- | --- | --- |
| 1. Hello | [Run](showcase/english/01-hello.slang) | [Kjør](showcase/norsk/01-hello.slang) | Variables, text input, blank/EOF handling, conditions, repetition |
| 2. Pocket money | [Run](showcase/english/02-pocket-money.slang) | [Kjør](showcase/norsk/02-pocket-money.slang) | Exact decimals, formatted prices, functions, ranges, large integers |
| 3. Weather measurements | [Run](showcase/english/03-weather.slang) | [Kjør](showcase/norsk/03-weather.slang) | Split text, typed lists, sorting, one-based indexing, statistics |
| 4. Pets | [Run](showcase/english/04-pets.slang) | [Kjør](showcase/norsk/04-pets.slang) | Object types, mutable fields, typed function parameters |
| 5. Counters | [Run](showcase/english/05-counters.slang) | [Kjør](showcase/norsk/05-counters.slang) | Closures, independent captured state, passing functions as values |
| 6. Shop library | [Run](showcase/english/06-library.slang) | [Kjør](showcase/norsk/06-library.slang) | Source imports, public types/functions, private helpers |
| 7. Notebook | [Run](showcase/english/07-notebook.slang) | [Kjør](showcase/norsk/07-notebook.slang) | UTF-8 files, persistence, checking existence, appending |
| 8. Guessing game | [Run](showcase/english/08-guess.slang) | [Kjør](showcase/norsk/08-guess.slang) | Randomness, input validation, while loops, continue/break |

The notebook creates/appends to **my-notebook.txt beside its source file**. Use a writable folder and run it twice to see persistence. The shop lesson imports `helpers/shop.slang`; keep the helper folder beside the lesson when copying it. Helpers are libraries, not standalone lessons.

The guessing game accepts only 1–6; other input gives a friendly message. Type `quit` in English or `slutt` in Norwegian to leave. EOF also exits cleanly. Norwegian decimal input/display uses `numbers.parse` and `numbers.format` with `","`; source literals keep `.`. `øre`, `blå`, and other Norwegian identifiers work normally. Library APIs, `print`, `input`, and `assert` retain their common names.

## Graphics and games

Expand **graphics → english** or **graphics → norsk**. These lessons use the same SDL library in the IDE canvas and native windows.

| Lesson | English | Norwegian | Controls and ideas |
| --- | --- | --- | --- |
| Bar chart | [Run](graphics/english/01-chart.slang) | [Kjør](graphics/norsk/01-chart.slang) | An implicit canvas, lists, coordinates, coloured rectangles; the console is the legend |
| Bouncing ball | [Run](graphics/english/02-bounce.slang) | [Kjør](graphics/norsk/02-bounce.slang) | A frame callback, elapsed time and wall collisions; hold Space to freeze, Escape to exit |
| Collect coins | [Run](graphics/english/03-coins.slang) | [Kjør](graphics/norsk/03-coins.slang) | An ordinary while loop, sprite, keyboard input, distance-based collisions and random targets |

**Click the IDE canvas before using game keys.** In the coin game, use the arrow keys to collect five yellow coins; scores and the victory message appear in the console. Escape exits. Keep `ship.png` in the parent graphics folder if you copy the game. The animation and game target the library's default 60 FPS and use elapsed seconds for movement; actual performance depends on the machine.

The chart draws once. In the CLI, close its window to finish; in the IDE, the program finishes while its picture remains in the canvas. To explore, change one value suggested by the `Try` comment at the top of each file, predict the result, and run again. Some `assert` lines check the original sample values; update those expectations when intentionally changing the data.

## Reference and licensing

For desktop forms, try the new [English greeting](windows/english/hello.slang) / [norsk hilsen](windows/norsk/hello.slang), then the [English club form](windows/english/club.slang) / [norsk klubbskjema](windows/norsk/club.slang). The club form demonstrates labels, inputs, buttons, checklists, radio groups, list boxes and combo boxes. See the [window guide](../docs/windows.md).

- [Standard libraries](../docs/standard-library.md), [language/runtime rules](../docs/runtime.md), and [graphics API](../docs/graphics.md).
- Console lessons and helper libraries use the project's [MIT license](../LICENSE).
- Everything original under `graphics/`, including these paired examples and the ship image, uses [CC0](graphics/README.md).

Developers can run `npm run examples:test` after building the native interpreter. It checks all 24 source files against both parsers, executes the console lessons with input, tests notebook persistence in a temporary folder, compares chart pixels, and drives the animation/game through the IDE protocol. The tests do not write into the source examples.
