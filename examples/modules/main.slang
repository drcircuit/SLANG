// Run this file from any working directory: imports are relative to this file.
use console as io
use numbers
use "maths.slang" as maths
use "./maths.slang" as same
use "players.slang" as players

io.print<"Square:", maths.Square<5>>
io.print<"Shared module state:", same.Calls<>>

var player as players.Player = players.Create<"Arne">
players.Heal<player, 10>
io.print<player.name, player.health>
io.print<"Rounded:", numbers.round<2.355, 2>>

// maths.Validate and maths.calls are private. Uncomment to see the diagnostic:
// maths.Validate<1>
