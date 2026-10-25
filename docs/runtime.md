# Running SLANG 0.5

The native executable runs source files and offers a persistent console. This milestone uses an AST interpreter to make the language usable and test its semantics. The planned bytecode compiler and static type inference are not implemented. This document records the initial execution rules; it takes precedence over older unresolved design notes in the draft specification.

## Diagnostic language

English and Norwegian names can be mixed in the REPL. Source files may put `#no` or `#en` on the first line to select diagnostic language. The directive overrides the IDE setting or native CLI `--locale no|en` default and does not restrict source vocabulary or rewrite program output. See [language settings and aliases](localization.md).

## Numbers

`int` uses arbitrary-size signed decimal coefficients, with memory as its practical bound. There is no fixed-width overflow or conversion through binary floating point. Decimal literals are parsed directly into a coefficient and scale. `+`, `-`, and `*` are exact; mixing `int` and `dec` produces `dec`.

`/` always returns `dec`, rounding to 28 places after the decimal point with ties to even. `divide<a, b, places>` chooses a different number of fractional places; `round<value, places>` rounds an existing value. Explicit rounding places must be an `int` from 0 through 10000, a resource limit for this preview. Display removes insignificant trailing zeroes: `2.00` prints as `2`, but `type<2.00>` is `"dec"`. Use `numbers.format<value, places, separator?>` for fixed decimal places and optional comma display; `numbers.parse<text, separator?>` reads decimal text. See [standard libraries](standard-library.md).

```text
print<0.1 + 0.2>                 // 0.3
print<round<2.345, 2>>           // 2.34
print<round<2.355, 2>>           // 2.36
print<divide<1, 8, 2>>           // 0.12
print<1 / 3>                    // 0.3333333333333333333333333333
```

Division or remainder by zero raises an error. `%` requires two `int` values and has the dividend's sign (`-7 % 3` is `-1`). Conversion to `int` rejects fractional decimals; round deliberately before converting. `int(text)` and `dec(text)` accept an optional sign and ordinary decimal digits, with a decimal point for `dec`. They reject whitespace, exponent syntax, and partially numeric text. Boolean conversions to numbers yield 0 or 1.

## Types and absence

Bindings infer a type from their first non-`none` value, or use an explicit annotation. They keep that type. `int` may widen to an explicitly required `dec`; text/number coercion is never implicit. `none` is allowed in all bindings in this preview. A binding initialized with only `none` establishes its type on its first later non-`none` assignment. An undeclared or uninitialized binding raises an error.

Function parameter bindings infer their type on each invocation. Typed overloads are preferred: each exact annotated match scores two, each other accepted annotated match scores one, and an unannotated parameter scores zero. The highest score wins; equal best scores are ambiguous. `none` can match any annotated parameter. Unannotated functions are checked as they execute; their bodies are not analyzed to select overloads. Return types stabilize on the first non-`none` result for each argument-type specialization. This is runtime enforcement, not complete static inference; conflicting return paths can therefore fail on a later call.

`text(value)` renders a value. `bool(value)` uses truthiness. `int`, `dec`, `text`, and `bool` conversions are supported; `list(value)` and `obj(value)` only validate an existing value of that category. Use `list<...>` and `obj<...>` to construct collections.

## Scopes and functions

Every block has a lexical scope. Names and misplaced `ret`, `break`/`stopp`, or `continue` are checked before execution, including inside uncalled functions. Declarations are visible throughout their block, but a variable cannot be read or assigned until its initializer has executed. An inner declaration shadows an outer name throughout that inner block. Function and object declarations are installed at block entry; recursion and mutually recursive functions work.

Duplicate declarations in a scope are errors, except functions with distinct overload signatures. Function/object declarations cannot be reassigned. Ordinary variables holding function values can be reassigned to other functions. Closures share captured bindings; separate calls of an outer function create separate captured storage. Loop iterations have fresh bindings.

```text
func Counter <>:
    var n = 0
    ret func <>:
        ret ++n
    .
.
var next = Counter<>
print<next<>, next<>, next<>>     // 1 2 3
```

## Lists and objects

Ordinary lists can hold mixed value types. `list of int` (or another element annotation) constrains every element and subsequent mutation, including mutation through an alias. Attaching a list annotation validates its current contents and applies that constraint to the shared list. Empty lists use `list<>`; a single-element list uses `list<value>`.

Objects have a fixed field set. Declared fields follow their annotations; anonymous fields infer stable binding types. Declared object types are nominal: separate declarations are distinct types even when their field layouts agree. An `obj` annotation accepts any object. Numeric indexing is one-based on lists and object fields in construction order. Named field access uses `dog.name`. Bounds errors and missing fields raise diagnostics. Text indexing is not implemented; `length<text>` counts Unicode scalar values, not bytes or grapheme clusters.

Assignment shares lists and objects. `append<list, value>` mutates a list and returns `none`. `remove<list, index>` removes and returns an item. `for item in list` visits a snapshot of the initial items; changes to the list do not change that iteration sequence. Referenced objects within the snapshot are still shared.

## Operators and control flow

Zero, `none`, `false`, empty text, and empty lists are falsy. Other values, including empty objects and functions, are truthy. `and`/`or` short-circuit and return a `bool`. `is` compares numeric values across `int`/`dec`; `false`, `0`, and `none` remain distinct. Text and booleans compare by value; lists, objects, and functions compare by identity. Ordering operators currently require numbers.

Operands, arguments, and assignment targets are evaluated left to right. Prefix updates return the new value and postfix updates the old value. Index expressions are evaluated once. If an assignment's right side removes list items, its already-evaluated index is checked again before writing.

Ranges include both endpoints, use `int` values, and default to step `1`. Descending ranges need a negative step. Zero steps are errors; a step pointing away from the endpoint produces no iterations. `repeat` requires a non-negative `int` count. `stopp` leaves only the nearest loop. Returning from a function without a value, or falling off its end, yields `none`.

## Console and builtins

Start `slang repl`, run `slang` without arguments, or double-click `SLANG.cmd` in the Windows package. The console retains values and functions and prints expression results except `none`.

| Form | Meaning |
| --- | --- |
| `print<values...>` | Print values separated by spaces and finish with a newline |
| `input<prompt>` or `input<>` | Read one line as text; return `none` at EOF |
| `length<value>` | Count text characters, list items, or object fields |
| `type<value>` | Return the type name as text |
| `append<items, value>` | Add a list item |
| `remove<items, index>` | Remove and return a list item |
| `round<value, places>` | Round with ties to even |
| `divide<a, b, places>` | Divide and round with ties to even |
| `assert<condition, message>` | Raise an error on a falsy condition; message is optional |
| `error<message>` | Always stop the current execution with a text message |
| `:help` | Show console help |
| `:cancel` | Discard unfinished source |
| `:submit` | Parse unfinished source now and report its error |
| `:reset` | Clear all session declarations and values |
| `:quit` | Exit |

Multiline blocks and calls are accepted, including blank lines inside unfinished blocks. Use `:submit` to force parsing and see an error in unfinished input, or `:cancel` to discard it. Earlier functions retain their defining source for diagnostics. Syntax/name/control-flow failures happen before runtime effects. Runtime failures do not roll back earlier output or mutations. A failed initializer can leave an uninitialized declaration; reset the session to redeclare it. Ordinary declarations cannot be redeclared without a reset; new distinct overloads can be added.

`assert` and `error` are builtins available without an import, including inside libraries. `assert` checks a condition; `error` always fails and requires exactly one `text` argument. Both report the source location, stop the current execution (including callers), and leave earlier effects intact. File execution exits with status 1; the interactive console returns to its prompt and retains the session.

```text
if age < 0:
    error<"Age cannot be negative">
.
```

## Preview limits

The interpreter defaults to 1,000,000 evaluated steps per file/submission. Change this with `--max-steps N` on `run` or `repl`; `0` disables the budget. Ctrl+C requests interruption and is checked between evaluated operations. A single very large numeric operation or blocked console input can delay interruption. User function nesting is limited to 32 calls, and combined expression/statement evaluation nesting to 128 frames, to protect the native stack. These are interpreter limits, not intended language limits.

Runtime heap objects and captured scopes are traced and collected during execution as well as at submission boundaries, including after errors. Active scopes, temporary values, suspended calls, and iteration snapshots are explicit roots; cyclic lists/objects/functions are supported. Loaded modules and their private scopes remain rooted until session reset. Collection is considered at statement/iteration boundaries after at least 1024 new heap objects since the last collection. Retained data still consumes memory. See [module rules](modules.md) for imports, [standard libraries](standard-library.md) for text, collections, maths, random, time and files, and [graphics](graphics.md) for frame loops and callbacks. Graphics presentations renew the execution budget, so long-running games can continue while stuck updates remain bounded. Deeply nested values are abbreviated during printing. Version 0.6 adds native execution controls for the Electron IDE: pause/resume, statement breakpoints, stepping, and variable summaries. Optimized arithmetic, bytecode execution, and static type inference remain future work.

Only Windows x64 packaging has been verified locally. No separate runtime, external C++ library, or network access is needed to use the packaged features.
