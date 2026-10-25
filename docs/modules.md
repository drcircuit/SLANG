# Libraries and modules

Version 0.4 implements library declarations and imports in the native interpreter and the JavaScript syntax reference. A library is a namespace with its own scope; it is not an object type and cannot be constructed with `<>`.

Norwegian declarations use `bibliotek`, `offentlig`, and `bruk`. Native library names and their functions also have [Norwegian aliases](localization.md). An alias imports the same cached native module; source libraries retain their own explicit exported names.

## Declare and import a library

Save this as `maths.slang`:

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

Save the importing program beside it:

```text
use "maths.slang" as maths
print<maths.Square<5>>
```

`maths.Square` is accessible. `maths.Validate` is private. Declarations inside `lib` are private by default; `public` is required for exports. Exporting a function does not export its locals or nested helpers. Libraries cannot read their importers' variables. All declarations belonging to an imported library must be inside its `lib` block.

The initial export forms are `public func` and `public obj`. `public var`, public imports, nested libraries, and public declarations outside the direct library body are rejected. All overloads of a function must have the same visibility, so marking one overload public cannot accidentally expose private overloads. The explicit `private` modifier is unnecessary and is not a keyword.

## Object types are separate from libraries

```text
lib players:
    public obj Player:
        name as text
        health as int
    .

    public func Create <name as text>:
        ret Player<name, 100>
    .
.
```

```text
use "players.slang" as players
var player as players.Player = players.Create<"Arne">
print<player.name>
```

Exported types work in variable, parameter, object-field, and `list of players.Player` annotations. Two aliases importing the same library refer to the same nominal object types. Module privacy does not introduce private object fields; fields retain their existing behavior. A library can intentionally return a value of a private type, but importers cannot name or construct that private type through the namespace. Inheritance remains deferred.

Namespaces expose named exports only. Numeric indexing, `obj(namespace)`, and calling a namespace are errors. Exports and the bindings introduced by `use` cannot be reassigned. A namespace can be stored in an ordinary variable, but access through that variable still enforces the same export boundary. `type<namespace>` returns `"lib"`.

## Loading and lifetime

- `use "relative/file.slang" as alias` imports a source file. The alias is required and may differ from the name inside `lib`. Paths are relative to the importing source file, not the terminal's working directory. Absolute paths are also supported. Paths are literal; no extension is added automatically.
- In the console and for stdin source, relative paths start at the working directory. A library's own imports are always relative to that library file.
- An imported file must contain exactly one top-level `lib` block. Ordinary programs do not need a wrapper. Programs and console submissions may also declare one inline `lib`, isolated from the surrounding program's variables.
- `use` is allowed at program level or directly inside a library, not inside functions or conditional/loop blocks. Imports are resolved in source order at block entry, before local initializers and ordinary statements. Place imports first for clarity.
- Source modules are cached by canonical file identity per interpreter session. Different aliases, relative spellings, and equivalent paths share initialization, private state, and exported object types. Imports do not copy source into the caller.
- Initialization runs once and may have effects. Prefer exported setup functions for actions such as opening a window. Successful dependencies remain initialized if a later import fails.
- Circular imports report the dependency chain. Import nesting is limited to 32 modules in this interpreter.
- Once a module has been read, a syntax, validation, or initialization failure is cached too. Reimporting it reports the original diagnostic without repeating initialization effects. `:reset` clears successful and failed module caches and allows edited libraries to reload. A missing/unreadable path is reported without binding its requested alias.
- Reusing an already-declared alias in the same scope is a duplicate declaration error. Use a different alias or reset the console. Library aliases are private within their containing library; imports are not automatically re-exported.

Errors in loaded code retain that file's source and line information, including when an exported function is called in a later console submission. Private-member access and qualified-type checks occur during runtime preparation/evaluation. `check` and `ast` remain syntax-only tools: they do not load dependencies or validate export availability.

## Bundled native libraries

Bare names resolve only bundled libraries. They do not search the working directory or download packages:

```text
use console as io
use numbers

io.print<"Rounded:", numbers.round<2.355, 2>>
```

| Library | Public functions |
| --- | --- |
| `console` | `print<values...>`, `input<prompt>` (prompt optional) |
| `numbers` | Exact numeric helpers, rounding, powers, square roots, sums, and means |
| `strings` | Unicode text length/slicing, search, replace, split/join, trim, Nordic case conversion |
| `lists` | Mutation, membership, shallow copying/slicing, sorting, reversing |
| `random` | Seeded random integers, choices, and shuffled copies |
| `time` | Elapsed seconds and interruptible waits |
| `files` | UTF-8 read, write, append, and existence checks |
| `window` | Labels, inputs, buttons, checklists, radio groups, list boxes, combo boxes and callbacks; see [windows](windows.md) |
| `graphics` | Canvas, frame loops/callbacks, shapes, keyboard input, sprites, and PNG export; see [graphics](graphics.md) |

`console.print/input` and `numbers.round/divide` expose the same native functions already available globally, preserving existing programs. New helpers are accessed through their libraries. Native exports use the same read-only namespace and access checks as source libraries. Unknown bundled names report an error. See the [standard library reference](standard-library.md) for signatures, examples, and limits. The IDE supplies curated signatures for every bundled export and scans public source exports; tests check the editor catalog against the native registry. Package management, a third-party binary-plugin ABI, and a native signature service remain future work.

The new keywords are `lib`, `public`, and `use` in both teaching languages for now. Existing Norwegian keywords work inside library bodies, and `som` can replace `as` for an import alias. Norwegian names for the three new keywords have not been selected.

Run `slang run examples/modules/main.slang` for a complete example with private helpers, private shared state, exported object types, aliases, and bundled native libraries.
