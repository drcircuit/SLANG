use graphics as g

g.canvas<640, 480, "Arrow keys move; Escape closes">
var x as dec = 300
var y as dec = 220

while g.open<>:
    var distance = 180 * g.seconds<>
    if g.key_down<"right">:
        x = x + distance
    .
    if g.key_down<"left">:
        x = x - distance
    .
    if g.key_down<"down">:
        y = y + distance
    .
    if g.key_down<"up">:
        y = y - distance
    .
    g.background<"#18243A">
    g.sprite<"ship.png", x, y, 48, 48>
    if g.key_down<"escape">:
        g.close<>
    .
    g.present<>
.
print<"Game finished.">
