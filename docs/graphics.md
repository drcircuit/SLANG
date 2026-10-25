# Graphics in SLANG 0.5

`use graphics as g` loads the bundled SDL graphics library. It adds no keywords or global builtins. Importing it does not open a window. SDL 3.4.18 is linked into the native executable; students do not install SDL or download anything.

## Draw immediately

```text
use graphics as g
g.fill<"blue">
g.circle<200, 150, 40>
g.line<50, 50, 300, 200>
```

The first drawing operation creates a 640 by 480 white canvas. The default drawing colour is black. Ordinary drawing persists until cleared, including across presentations, window repainting, and resizing. At successful program completion, the runner presents the result and keeps its window responsive until closed. `print` still writes to the console. In the REPL, each submission updates the existing canvas, and the window stays responsive while the console waits for input.

`g.canvas<800, 600, "My drawing">` explicitly creates a fresh canvas, replacing the old one and its cached sprites. This preview supports one canvas per runtime. Window resizing scales and letterboxes its fixed logical drawing area; it does not change drawing coordinates. Coordinates start at the top left, with x rightward and y downward.

## Use an ordinary loop

```text
use graphics as g

var x as dec = 100
while g.open<>:
    if g.key_down<"right">:
        x = x + 120 * g.seconds<>
    .
    g.background<"white">
    g.circle<x, 150, 20>
    g.present<>
.
```

`open<>` creates the implicit canvas if needed, processes events, snapshots keyboard input and elapsed time, and begins a frame. It returns false after the canvas closes. `present<>` displays the completed frame and paces execution, targeting 60 FPS by default. Call it once per iteration. Calling `open<>` again before completing the frame is an error, including when a `continue` skips `present<>`.

`seconds<>` returns a `dec`, in seconds since the preceding frame began. Its first value is the target frame interval; later values are capped at 0.1 seconds so a pause or window drag does not cause an enormous movement jump. The value stays constant within a frame. Use `as dec` for positions updated using elapsed time: an inferred `int` binding cannot later receive fractional coordinates.

Existing `repeat`, `for`, `break`, and `continue` retain their meanings. `present<>` can also complete a frame in a `repeat`/`for` loop without an explicit `open<>`, but `while g.open<>` is recommended for interactive input. `break` leaves the loop without closing the drawing; `g.close<>` closes it explicitly. Code after the loop continues normally.

## Or pass a function

```text
use graphics as g

var x as dec = 100
func Frame <seconds as dec>:
    if g.key_down<"right">:
        x = x + 120 * seconds
    .
    g.background<"white">
    g.circle<x, 150, 20>
.

g.run<Frame>
```

`run` uses the same frame lifecycle and passes elapsed seconds to the function. It manages opening, input, presenting, and pacing. A callback is an ordinary function value and may be a closure. Its return value is ignored. `run` returns when the canvas closes. Callbacks must not call `open`, `present`, or another `run`; configure the canvas before starting the callback loop. They may call `close` and then return. A callback error stops execution, reports the original source location, and closes the canvas.

## Functions

| Function | Meaning |
| --- | --- |
| `canvas<width, height, title>` | Fresh canvas; title optional; dimensions are ints from 1 to 4096 |
| `open<>` | Start a frame; false if closed |
| `seconds<>` | Elapsed time snapshot as dec |
| `present<>` | Display and pace one frame |
| `run<callback>` | Run frames using a one-argument function |
| `close<>` | Close the canvas and release its textures |
| `fps<rate>` | Target frame rate, int from 1 to 240; default 60 |
| `key_down<name>` | Whether a key is held in this frame's snapshot |
| `background<color>` | Clear the entire canvas to a colour |
| `fill<color>` | Set the colour for subsequent shapes and lines; does not open a window |
| `line<x1, y1, x2, y2>` | Draw a one-pixel line |
| `rect<x, y, width, height>` | Draw a filled rectangle from its top-left corner |
| `circle<x, y, radius>` | Draw a filled circle centred at x/y |
| `sprite<path, x, y>` | Draw a PNG/BMP at its natural size from its top-left corner |
| `sprite<path, x, y, width, height>` | Draw a scaled sprite; nearest-neighbour scaling |
| `save<path>` | Save the logical canvas as a PNG file |

Colours are case-insensitive English names (`black`, `white`, `red`, `green`, `blue`, `yellow`, `orange`, `purple`, `pink`, `gray`/`grey`, `cyan`, `magenta`) or Norwegian names (`svart`, `hvit`, `rød`, `grønn`, `blå`, `gul`, `oransje`, `lilla`, `rosa`, `grå`, `cyan`, `magenta`). Both work with `fill` and `background`, including uppercase Nordic letters such as `"RØD"`. Hex colours such as `"#AABBCC"` still work. The IDE suggests colour names in the selected language; switching languages does not rewrite existing strings. See the [colour translation table](localization.md#fargenavn).

Sprite transparency is supported. Shape dimensions and radii cannot be negative. Coordinates accept int/dec values between -10000000 and 10000000 and are converted to native rendering coordinates only at the library boundary; SLANG arithmetic remains unchanged.

Key names include `"right"`, `"left"`, `"up"`, `"down"`, `"space"`, `"escape"`, and letters. Unknown keys report an error. These are physical key names for game controls, not a text-entry API. Pending key changes are applied at the next frame boundary, so all input checks in one frame are consistent.

Sprite and save paths resolve relative to the source file making the call, including library functions; console paths use the working directory. Sprites are decoded and uploaded on first use, then cached per canvas using the normalized absolute path. Repeated draws do not reread the file. Open a fresh canvas or reset the session to reload a changed image. `save` writes or replaces the named PNG; its parent directory must already exist.

## Lifetime, limits, and performance

Closing a canvas does not silently reopen it. Use `canvas` to explicitly start another, or `:reset` in the REPL. Reset and runtime destruction release all graphics resources. Runtime errors close an active canvas. Ctrl+C interrupts execution or the final window wait. Events are also serviced during long interpreter loops; closing a window during a stuck update stops that execution with a diagnostic. A large native arithmetic operation can still delay cancellation until it completes.

The execution-step budget applies between presented frames. A successful presentation renews it, allowing a game to run indefinitely while retaining protection against an update that never finishes. The interpreter now traces active scopes, temporary values, and suspended calls and collects unreachable objects during execution. Retained lists, closures, and sprite textures still use memory deliberately. Drawing and event handling stay on the runner's main thread; console reads use a helper thread while a canvas is open.

Sustained 60 FPS for modest 2D games is the target, not a guarantee across machines. `examples/graphics/sprite-benchmark.slang` renders 120 moving sprites at 960 by 540 for 300 frames, reporting average FPS, worst frame, and frames over 20 ms. Run it on representative school hardware. It measures rendering plus simple interpreter updates, not a complete collision/physics workload. Reported elapsed time uses the same 0.1-second clamp as gameplay; use an external profiler for stalls longer than that.

The current backend uses SDL's renderer selection, with software fallback where available. This is a basic 2D library: plotting, text rendering/fonts, mouse input, sound, sprite-sheet animation, multiple canvases, and 3D are not yet implemented. Version 0.6 embeds the same SDL renderer in the [Electron IDE canvas](ide.md), with console output beside it. The CLI retains its native window. A finished IDE drawing leaves its final image in the panel; keyboard input is forwarded while the canvas has focus. The preview can skip frames under transfer load while the native simulation continues.

Examples: `drawing.slang`, `loop.slang`, `callback.slang`, and `sprite-benchmark.slang` under `examples/graphics`. The demo ship is an original procedural pixel sprite included with the project.
