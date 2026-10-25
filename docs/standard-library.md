# Standard libraries for classroom programs

These libraries are built into the native interpreter and work offline in the CLI, interactive console, and IDE. Import just the namespaces a lesson uses. The IDE supplies member completion and parameter hints, including when a library has an alias.

```text
use random
use numbers as maths

var rolls = list<>
repeat 10 times:
    append<rolls, random.integer<1, 6>>
.
print<"Average roll:", maths.mean<rolls>>
```

Every standard library and function has a Norwegian name for the same implementation, such as `bruk tall som matte` and `matte.gjennomsnitt<verdier>`. See the complete [English/Norwegian name tables and language settings](localization.md). See [modules](modules.md) for source libraries and visibility, [runtime](runtime.md) for types, and [graphics](graphics.md) for drawing and games.

For forms with labels, inputs, buttons, checklists, radio groups, list boxes and combo boxes, use the [window library](windows.md). It shares the native SDL window/IDE canvas with graphics and provides automatic layout and button callbacks.

## Common rules

- Arguments have the same strong type checks as ordinary operations. Text is not implicitly converted to numbers; use `int(value)` or `dec(value)` explicitly.
- Indices start at **1**. `find` returns **`none`** when no match exists. `slice<value, start, count>` uses a starting index and a number of items, not an ending index. Omitting count takes the remaining items. Starting at `length + 1` is allowed for an empty slice; negative indices, zero starts, and slices beyond the end are errors.
- Text positions count Unicode scalar values. `Æ`, `ø`, `å`, and an ordinary emoji each occupy one position. Combining marks and multi-character emoji sequences can occupy several positions. Searches are exact and case-sensitive, without Unicode normalization.
- List copies are shallow: the outer list is new, but nested lists and objects remain shared. Copies retain any `list of ...` constraint. `lists.append` and `lists.remove` mutate; `copy`, `slice`, `sort`, `reverse`, and `random.shuffle` return new lists.
- Functions take positional arguments. Wrong argument counts, types, ranges, and I/O failures give ordinary source-located errors. `assert` is for checking results; it does not catch errors or undo prior effects.

## Numbers

`use numbers` provides:

| Function | Result |
| --- | --- |
| `abs<value>` | Absolute value, keeping `int` or `dec` |
| `min<a, b>`, `max<a, b>` | Smaller or larger number |
| `clamp<value, minimum, maximum>` | Number limited to inclusive bounds; minimum must not exceed maximum |
| `floor<value>`, `ceil<value>` | Round down or up, returning `int`; `floor<-1.2>` is `-2` |
| `pow<base, exponent>` | Exact power with a non-negative `int` exponent; `pow<0, 0>` is `1` |
| `sqrt<value, places?>` | Decimal square root; value must be non-negative; default 28 fractional places |
| `sum<items>` | Exact total of a numeric list; empty lists total `0` |
| `mean<items>` | Decimal arithmetic mean of a non-empty numeric list, rounded to 28 fractional places |
| `round<value, places>` | Decimal rounding with ties to even |
| `divide<a, b, places>` | Decimal division with explicit precision and ties to even |
| `parse<value, separator?>` | Parse text as `dec`; separator is `"."` by default, or explicitly `","` |
| `format<value, places, separator?>` | Text with a fixed number of decimal places and the chosen separator (default `"."`) |

Integers remain arbitrary precision. These helpers do not pass values through binary floating point. `sqrt` uses decimal/integer arithmetic and rounds to nearest, with ties to even. `min`, `max`, and `clamp` return `dec` if any argument is `dec`; `pow` retains its base's numeric type. Explicit places and exponents are limited to 0–10000 in this preview. Large powers and high-precision roots can be expensive; a single arithmetic operation can delay interruption. Trigonometry, grouping separators, and automatic currency symbols are not included yet.

Norwegian letters work in identifiers. Decimal commas are supported in **input and display**, explicitly, so a program behaves the same on every computer. Source literals still use a dot: `var øre = 0,01` is a list of two integers under the existing comma-separated list grammar. `parse` strips surrounding Unicode whitespace and accepts an optional sign, digits, and the selected decimal separator. It rejects grouping separators, exponent notation, and partial numbers. `format` rounds with ties to even and pads trailing zeros.

```text
use numbers as tall
var øre = 0.01
var pris = tall.parse<"12,50", ",">
print<tall.format<pris + øre, 2, ",">>   // 12,51
```

```text
use numbers as maths
var measurements = 12.4, 12.6, 12.5
print<maths.mean<measurements>>       // 12.5
print<maths.sqrt<2, 4>>               // 1.4142
print<maths.sum<list<0.1, 0.2>>>      // 0.3
```

## Strings

`use strings` provides:

| Function | Result |
| --- | --- |
| `length<value>` | Number of Unicode scalar values in text |
| `contains<value, search>` | Whether the search text occurs |
| `find<value, search>` | First one-based position, or `none` |
| `slice<value, start, count?>` | Copy a portion of text |
| `replace<value, search, replacement>` | Replace all non-overlapping matches |
| `split<value, separator>` | Split into `list of text`, retaining empty parts |
| `join<items, separator>` | Join a list of text; convert numbers explicitly before adding them |
| `trim<value>` | Remove Unicode whitespace at both ends |
| `lower<value>`, `upper<value>` | Change basic Latin and Latin-1 letter case, including Nordic letters |

`contains<value, "">` is true and `find<value, "">` is 1. `split` and `replace` require non-empty separator/search text. `split<"", ",">` produces one empty item, and a trailing separator produces a final empty item. Splitting on newline preserves any carriage return in Windows line endings; apply `trim` to each field when appropriate. `replace` and `join` limit their output to 16 MiB; `split` limits its result to 1,000,000 items.

Case conversion is deterministic and does not depend on the computer's locale. This initial implementation covers basic Latin and Latin-1 (`æøåäöé`, Icelandic letters, etc.), plus `ÿ`/`Ÿ`; uppercasing `ß` produces `SS`. Other scripts pass through unchanged. This is not complete Unicode case folding or language-aware casing.

```text
use strings as s
var names = s.split<"Arne,Siri,Åse", ",">
print<s.join<names, " and ">>     // Arne and Siri and Åse
print<s.upper<"blåbær">>          // BLÅBÆR
print<s.slice<"Æøå🐟", 2, 2>>     // øå
```

## Lists

`use lists` provides `length<items>`, `append<items, value>`, `remove<items, index>`, `contains<items, value>`, `find<items, value>`, `copy<items>`, `slice<items, start, count?>`, `sort<items>`, and `reverse<items>`.

`append` returns `none`; `remove` returns the removed item. Membership/search uses the same equality as `is`: numbers compare by numeric value, while objects and lists compare by identity. Sorting is stable and accepts either all numbers (mixing `int` and `dec` is fine) or all text. Text sorts by Unicode scalar order, **not Norwegian/Swedish/Danish dictionary order**. Other mixtures and `none` elements are errors when sorting.

```text
use lists
var scores = 8, 3, 10, 5
var ordered = lists.sort<scores>
print<ordered>                     // list<3, 5, 8, 10>
print<scores.1>                    // 8; source is unchanged
```

## Randomness

`use random` provides:

| Function | Result |
| --- | --- |
| `integer<minimum, maximum>` | Uniform random `int`, both bounds included |
| `choice<items>` | One item from a non-empty list |
| `shuffle<items>` | A shuffled shallow copy, including empty lists |
| `seed<value>` | Set an arbitrary `int` seed; return `none` |

Each interpreter session owns its generator, shared by all aliases of `random`. Without `seed`, the generator starts from system entropy. Reset clears its state. Setting the same seed and making the same calls reproduces an exercise across supported platforms with this implementation; future generator changes may change the sequence. Seeds may be negative. Integer bounds may exceed machine sizes; their difference is limited to 10000 decimal digits. This generator is for games and simulations, not passwords or cryptography.

```text
use random
random.seed<42>                  // Optional: reproduce the same exercise
print<random.integer<1, 6>>
print<random.choice<list<"red", "blue", "green">>>
```

## Time

`use time` provides `seconds<>`, monotonic decimal seconds since the interpreter session started/reset, and `sleep<seconds>`, a wait accepting `int` or `dec` between 0 and 86400. Sleep rounds to microseconds; operating-system scheduling determines actual timing. Zero returns immediately after servicing events. Stop, Pause, and canvas events are serviced during a wait; time spent paused counts toward the wait. `seconds` also includes time spent paused and is not a calendar clock.

For games, use `graphics.present` or `graphics.run` for frame pacing. Their existing `graphics.seconds<>` measures a frame interval, whereas `time.seconds<>` measures elapsed session time.

## Files

`use files` provides:

| Function | Effect |
| --- | --- |
| `exists<path>` | Test whether a file or directory exists |
| `read<path>` | Read a regular file as UTF-8 text, up to 16 MiB |
| `write<path, content>` | Create or **overwrite** a text file; return `none` |
| `append<path, content>` | Append text, creating a file if needed; return `none` |

Paths resolve relative to the source file containing the call. A helper function in an imported module uses that module's directory. Console/stdin submissions use the working directory. Absolute paths are supported; `..` and symlinks follow ordinary OS rules. This is normal filesystem access with the user's permissions, not a filesystem sandbox. Use `/` in portable source paths, including on Windows.

Parent directories must already exist. Reading a missing file, using a directory as a file, invalid UTF-8, and permission failures are errors. `exists` returns false for missing paths and reports access errors. A UTF-8 BOM is removed on read; line endings are otherwise preserved. Writes emit UTF-8 without adding a BOM or newline. Content is validated before opening for writing. Each write/append accepts at most 16 MiB, but appending can make a file larger than the read limit. Writes are not transactional: an OS write failure can leave a partial file. Delete, directory manipulation, binary streams, and structured CSV/JSON APIs are deferred.

```text
use files
files.write<"notes.txt", "Today I learned about lists.\n">
files.append<"notes.txt", "Tomorrow: games!\n">
print<files.read<"notes.txt">>
```

## Try the examples

- `examples/standard-library/dice.slang`: roll dice, sort, and calculate an average.
- `examples/standard-library/word-game.slang`: random words and Scandinavian text operations.
- `examples/standard-library/measurements.slang`: parse measurements and calculate statistics.
- `examples/standard-library/notebook.slang`: write, append, and read `student-notes.txt` beside the example.
- `examples/standard-library/norsk.slang`: Norwegian syntax with the same libraries.

Run with `slang run <path>` or open an example in the IDE and press **Run**. The libraries are compiled into the interpreter; no package download or extra runtime is needed.
