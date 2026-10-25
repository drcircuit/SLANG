// CC0. Lesson: a small game using an ordinary while loop, sprites and collision.
// Arrow keys move. Collect 5 coins. Escape closes. Click the IDE canvas first.
// Try: change the target, ship speed, or coin colour.
use graphics as g
use numbers as maths
use random

g.canvas<640, 480, "Collect 5 coins | Arrow keys move | Escape closes">
var x as dec = 60
var y as dec = 240
var coin = obj<x = 320, y = 240>
var score = 0
var target = 5
print<"Collect the yellow coins! Score appears here.">

while g.open<>:
    var distance = 240 * maths.min<g.seconds<>, 0.05>
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
    x = maths.clamp<x, 16, 624>
    y = maths.clamp<y, 16, 464>

    var dx = x - coin.x
    var dy = y - coin.y
    if dx * dx + dy * dy < 24 * 24:
        score++
        print<"Coins:", score, "/", target>
        if score is target:
            print<"You won!">
            g.close<>
            break
        .
        coin.x = random.integer<80, 560>
        coin.y = random.integer<80, 400>
    .
    g.background<"#18243A">
    g.fill<"#FFD670">
    g.circle<coin.x, coin.y, 8>
    g.sprite<"../ship.png", x - 16, y - 16, 32, 32>
    if g.key_down<"escape">:
        g.close<>
        break
    .
    g.present<>
.
print<"Game finished.">
