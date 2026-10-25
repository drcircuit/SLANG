# SLANG draft specification — 0.5

Status: working design with a native C++20 frontend and interpreter, plus a JavaScript syntax reference. The design below retains intended static typing and some open questions. [Runtime rules](runtime.md), [module rules](modules.md), and [graphics](graphics.md) record current implementation choices. Version 0.6 adds an [Electron IDE](ide.md) with code completion, console/canvas panels, and basic debugging. The bytecode compiler and full static type inference remain future work.

## Purpose

SLANG is intended for school-age beginners and teachers, with comfortable typing on Scandinavian keyboards, predictable explanations, and concepts that transfer to other languages. It should support procedural, functional, and object-oriented styles, and be sufficient for useful small programs, graphics, and small games. The selected production direction is a native C++20 core and bytecode engine, with a CLI and interactive interpreter, plus an Electron IDE. Runtime performance targets remain open. The product should combine an integrated edit/build/run experience inspired by Turbo Pascal with interactive exploration inspired by Python.

The finished distribution must work after extraction or installation with all required components included. It must not require a separate Node.js, .NET, Python, compiler-toolchain, or package-manager installation. Compiler-based completion must not use AI. See [the product roadmap](roadmap.md) for the distribution contract and implementation stages. These product decisions do not imply that those components have already been built.

## Established direction

### Blocks and statements

Blocks begin with `:` and finish with a standalone `.`. The closing period may have a trailing comment. Indentation has no semantic effect. Line breaks separate statements; expressions may continue across lines inside calls, conversions, and grouping parentheses. These newline and grouping rules are provisional grammar details implemented by the prototype.

```text
while window_open<>:
    if key_down<"right">:
        x++
    .
.
```

One period closes the entire `if` / `else if` / `else` construct. Comments use `//` in the prototype. Text uses double quotes and JSON-style escapes. Unicode identifiers, including Norwegian letters, are supported; keywords are lowercase and case-sensitive. Parentheses are also available for mathematical grouping, not only conversion.

### Variables, types, and absence

Variables require declarations. The intended type system is strong and static, with inference for variables, function parameters, and return values where the program supplies sufficient constraints. Inference does not mean that an established variable type can change arbitrarily.

```text
var score = 10
var name as text = "Ada"
```

The core vocabulary is:

| Type | Intended meaning |
| --- | --- |
| `int` | Arbitrary-precision signed integer, bounded by resources rather than a fixed source-language width |
| `dec` | Built-in accurate decimal arithmetic, with directly parsed decimal literals |
| `text` | Unicode text |
| `bool` | Boolean values `true` and `false` |
| `list` | Mutable, ordered collection |
| `obj` | Structured object with named or positional fields |

`none` denotes absence, distinct from `0`, `false`, and empty text. An undeclared name is an error, not an absent value. The interpreter currently permits absence in every binding, and infers a type on the first non-`none` value. The final static type representation remains open.

The prototype accepts annotations on variables, parameters, and declared object fields, including the provisional form `list of int`. It requires initializers on ordinary variable declarations for now. Explicit return annotations and uninitialized declarations have not been specified.

### Numbers and conversion

The interpreter implements arbitrary-size integers and decimal arithmetic without binary floating-point conversion. Finite decimal addition, subtraction, and multiplication are exact within available resources.

The initial runtime policy uses decimal division with 28 fractional places and ties-to-even rounding; explicit `round` and `divide` helpers select a different number of places. Division by zero and invalid conversions raise errors. These choices remain open to language-design review. Decimal arithmetic alone does not settle all financial rounding requirements. Decimal commas are not supported: `1, 5` is a list and `1.5` is a decimal.

Conversion uses a destination type followed by parentheses:

```text
text(42)
int("42")
dec("19.95")
```

Parsing a conversion does not establish that it is valid. The interpreter enforces the conversion rules described in [runtime rules](runtime.md).

### Functions, overloads, and closures

```text
func Add <a, b>:
    ret a + b
.

func Add <a as int, b as text>:
    ret text(a) + b
.

var numeric = Add<2, 3>
var label = Add<2, "3">
```

The unannotated function is intended to support compatible inferred types. `+` performs numeric addition or text concatenation. It does not implicitly concatenate a number with text. An explicitly typed overload can implement that combination by converting it deliberately.

The working overload rule prefers an exact typed match to a compatible generic candidate and rejects unresolved ambiguity. Full ranking rules, partially annotated overloads, recursion, and recursive inference remain open. Function return types are inferred in the current syntax.

Functions are values. Referring to `Next` yields the function; `Next<>` calls it.

```text
func MakeCounter <>:
    var count = 0
    func Next <>:
        count++
        ret count
    .
    ret Next
.

var counter = MakeCounter<>
var double = func <value>:
    ret value * 2
.
```

Lexical scopes and closures now execute with shared mutable bindings within a scope, separate bindings for separate outer calls, and fresh loop bindings per iteration. Captured storage survives as long as a reachable closure needs it. The interpreter traces its heap during execution and at submission boundaries, including active evaluator roots. Bytecode capture analysis remains future work.

### Lists and objects

```text
var values = 1, 2, 3, 4
var words = "one", "two", "three"
var empty = list<>
var single = list<1>
```

Comma-separated values in an initializer form a list; commas in angle-delimited calls separate arguments. Explicit list construction permits a list to be passed as one argument: `process<list<1, 2, 3>>`. Comma-separated returns and iteration sources are also accepted by the provisional parser.

```text
obj Animal:
    legs as int
    furry as bool
    name as text
.

var dog = Animal<4, true, "Rex">
var named = obj<age = 1, legs = 2, name = "Arne">
var positional = obj<1, 2, "Arne">
```

Named object constructors take fields in declaration order. Anonymous objects may have named or positional fields; positional objects do not require a separate tuple type. The prototype rejects mixing named and positional initializers in one anonymous object. Field initializers use `=` syntactically, without making ordinary assignment an expression.

```text
values.1
values.<i>
values.<i + 1> = 99
dog.name
positional.3
```

Numeric indexing starts at 1. `a.<i>` evaluates `i` as the index; `dog.name` refers to the literal field name. Static index zero is rejected by the parser. The interpreter checks computed indices and bounds at runtime.

Initial runtime choices: general lists can contain mixed types; `list of T` enforces an element type through aliases. Assignment shares collection objects, and iteration uses a snapshot. `append` and `remove` mutate lists. Objects have fixed mutable fields and allow numeric access in construction order. Invalid indices raise errors. These choices are detailed in [runtime rules](runtime.md) and remain subject to design review.

### Operators, truthiness, and evaluation order

`=` assigns. `is` compares values and `is not` tests inequality. `==` and `!=` are rejected with suggestions. Numeric comparisons use `<`, `>`, `<=`, and `>=`. Logical operators are `and`, `or`, and `not`.

Numeric zero, `none`, `false`, empty text, and empty lists are falsy in the interpreter; other values are truthy. Truthiness does not make `0`, `false`, and `none` equal. Logical operators short-circuit and return booleans. Numeric equality compares values across `int` and `dec`; lists/objects compare by identity. These initial rules remain subject to design review.

Prefix and postfix updates have their conventional values:

| Form | Change | Result |
| --- | --- | --- |
| `++a` | Add one | Updated value |
| `a++` | Add one | Previous value |
| `--a` | Subtract one | Updated value |
| `a--` | Subtract one | Previous value |

The interpreter permits updates to mutable numeric variables, fields, and indexed items, with left-to-right operand and argument evaluation. Assignment remains a statement; updates are expressions. Type checks occur during execution.

Provisional precedence, highest first:

1. Grouping, conversion, calls, field/index access.
2. Postfix updates.
3. Prefix updates and unary `+` / `-`.
4. `*`, `/`, `%`.
5. `+`, `-`.
6. Comparisons and equality.
7. `not`.
8. `and`.
9. `or`.

Arithmetic binary operators associate left-to-right. Chained comparisons require explicit grouping or separate comparisons joined with `and`. `%` requires integers and returns a remainder with the dividend's sign.

### Control flow

The following syntax has been proposed and is parsed:

```text
for name in names:
    print<name>
.

for i from 1 to 5:
    print<values.<i>>
.

for i from 5 to 1 step -1:
    print<i>
.

repeat 3 times:
    print<"Hello">
.
```

Ranges include both endpoints. `break` leaves the nearest loop and `continue` starts its next iteration. The interpreter uses integer endpoints and a default step of 1, rejects zero steps and fractional repeat counts, and iterates snapshots of lists. Execution preflight rejects loop controls outside loops and returns outside functions. The syntax-only `check` command does not perform those checks.

## Libraries and visibility

`lib name:` opens a library's isolated scope and namespace. A standalone period closes it, using the same block rules as functions and control flow. Declarations inside the library are **private by default**. Only declarations explicitly marked `public` are exported. Being inside a `lib` block does not itself make a declaration public. No explicit `private` modifier is needed for the initial design.

```text
lib maths:
    func Validate <val>:
        assert<val >= 0>
    .

    public func Square <val>:
        Validate<val>
        ret val * val
    .
.
```

Importers can access `maths.Square`, but cannot access `maths.Validate`. Both functions can use declarations in their own library scope. Locals and nested helpers inside an exported function remain local; exporting a function does not export its contents.

Module visibility is separate from object-member visibility. Exporting an object type makes that type available to importers; it does not introduce private fields, inheritance, or method visibility. Object fields retain their current rules. Inheritance is deferred and is not required for libraries.

Imported files contain exactly one top-level `lib` block, with public functions and object types and private library state. Ordinary programs do not need a wrapper and may also declare one inline library. `use "maths.slang" as maths` imports a local file relative to the importing source; `use console` imports a bundled library. Imports bind a read-only namespace and share one initialized module instance per program or console session. An optional alias renames a bundled import, as in `use console as io`. File imports require an explicit alias. Norwegian spellings for `lib`, `public`, and `use` have not been selected.

These forms execute in version 0.4. Only functions and object types can be public; all overloads of a name must share visibility. Exported object types can be used in qualified annotations, such as `players.Player`. Imported modules cannot see caller variables, and their private imports/state/helpers are not re-exported. Cycles are diagnosed, initialization results/failures are cached, and console reset clears loaded modules. See [module rules](modules.md) for the loading contract and bundled `console`/`numbers` APIs.

## English and Norwegian surface syntax

Localization uses keyword tokens and grammatical context, not source-text replacement. Both languages produce the same AST and share the type system and runtime design. Identifiers, object field names, and string contents retain their spelling. Existing code must not acquire different semantics when the teaching language changes.

Requested mappings are `if` → `hvis`, `from` → `fra`, `to` → `til`, `func` → `funk`, both `else` and `or` → `eller`, and `break` → `stopp`. These simple spellings target children through junior high school. Other Norwegian spellings below are provisional:

| English | Norwegian |
| --- | --- |
| `var` | `var` |
| `func` | `funk` |
| `ret` | `retur` |
| `obj` | `obj` |
| `if` | `hvis` |
| `else` | `eller` |
| `while` | `mens` |
| `for` | `for` |
| `in` | `blant` |
| `from` | `fra` |
| `to` | `til` |
| `step` | `steg` |
| `repeat` | `gjenta` |
| `times` | `ganger` |
| `break` | `stopp` |
| `continue` | `fortsett` |
| `as` | `som` |
| `of` | `av` |
| `and` | `og` |
| `or` | `eller` |
| `not` | `ikke` |
| `is` | `er` |
| `true` | `sann` |
| `false` | `usann` |
| `none` | `ingen` |
| `int` | `heltall` |
| `dec` | `desimal` |
| `text` | `tekst` |
| `bool` | `bool` |
| `list` | `liste` |

`is not` becomes `er ikke`; `else if` becomes `eller hvis`. `blant` is provisional to avoid reserving the common loop variable `i`. A contextual Norwegian `i` could be considered later.

`eller` means logical `or` between expressions, including inside conditions and function arguments. At a branch boundary, `eller:` introduces the fallback branch and `eller hvis condition:` introduces another conditional branch. This distinction follows the grammar and does not depend on indentation or syntax highlighting. English `else` and `or` remain separate keywords. `stopp` leaves the nearest loop; it does not terminate the program. The previous proposals `ellers` and `bryt` are not keywords.

```text
mens sann:
    hvis ready eller waiting:
        print<"Continue">
    eller hvis retry:
        print<"Try again">
    eller:
        stopp
    .
.
```

The prototype permits mixed keyword spellings and reserves all aliases. A future source-language setting could reduce reserved-name collisions. Library aliases, Norwegian diagnostics, keyword rendering in the editor, and complete Bokmål/Nynorsk policies are not implemented. Aliases do not automatically translate API names such as `print`.

## Angle-bracket grammar: remaining design issue

These examples parse with the intended structure:

```text
Add<2, Multiply<3, 4>>
check<a > b>
check<Add<a, b> > Multiply<c, d>>
Add<2, 3> > 4
```

Some token sequences have more than one valid expression tree:

```text
check<a < b>
```

This can mean `check` called with the comparison `a < b`, or a comparison between `check` and the call `a<b>`. Highlighting alone cannot decide between those interpretations. In the prototype, write `check<(a < b)>` for the first interpretation or `check < (a<b>)` for the second.

Another example is `f<a > +b> + c`. It could mean `f<(a > +b)> + c` or `(f<a> + b) > +c`.

The prototype explores expression alternatives, respects operator precedence, and rejects remaining ambiguity. A complete expression stops at the earliest valid statement boundary so speculative calls cannot consume later statements. This is a conservative way to test the design, not a production performance strategy. Before freezing the grammar, decide whether to retain explicit grouping in ambiguous cases, introduce a deterministic syntactic rule, or defer resolution until name/type analysis. Spaces and indentation do not distinguish calls from comparisons.

## Implementation boundaries and next milestones

Implemented in the native C++20 core: keyword normalization, UTF-8 tokenization, parsing, source spans, syntax diagnostics, AST output, name/control-flow validation, and an AST interpreter with runtime type checks, exact arithmetic, closures, lists, objects, overloads, and basic I/O. The native CLI supports file execution, a persistent console, and syntax-only JSON checks. CMake builds and ZIP packaging are available, with Windows x64 verified locally. The original JavaScript parser remains a syntax conformance reference.

Native source offsets count UTF-8 bytes; lines and Unicode-scalar columns start at 1. Text must contain valid Unicode: malformed UTF-8 and isolated surrogate escapes are rejected. This is stricter than JavaScript's acceptance of isolated surrogate escapes. A checked-in generated Unicode category table supports identifiers without a runtime dependency on Node or platform locale. The current alternative-exploring expression parser has nesting and search limits, reported as diagnostics, and remains a frontend prototype rather than a final performance design.

Not implemented: full static type inference, bytecode compilation, a production VM, general file APIs, plotting, sound, IoT, or an editor. Source modules, an SDL 2D graphics library, collection during execution, and a small native library registry are implemented; third-party binary plugins and a complete library-signature service are not. Current execution rules and resource limits are documented in [runtime rules](runtime.md).

Next milestones:

1. Complete platform verification of the implemented native C++20 core and build; retain syntax conformance tests during subsequent changes.
2. Extend implemented name/control-flow validation with full static type inference.
3. Review the initial absence, mixed-list, and overload rules against classroom examples.
4. Review the implemented decimal division/rounding and conversion policy.
5. Implement bytecode execution and optimize collection, preserving file/console/graphics runtime conformance tests.
6. Expose language services and execution controls for an Electron IDE with non-AI completion.
7. Add standard libraries in stages and validate complete offline distributions on supported Windows, macOS, and Linux systems.

The language now has a runnable interpreter preview. The language specification and complete educational product remain under development.
