// CC0. Lesson: animation with a function callback and elapsed seconds.
// Hold Space to freeze the ball; Escape closes. Try a different speed.
use graphics as g

g.canvas<640, 480, "Bouncing ball | Hold Space to freeze | Escape to close">
var x as dec = 70
var y as dec = 90
var speedX = 180
var speedY = 130

func Frame <seconds as dec>:
    if not g.key_down<"space">:
        x = x + speedX * seconds
        y = y + speedY * seconds
        if x > 620:
            x = 620
            speedX = -speedX
        .
        if x < 20:
            x = 20
            speedX = -speedX
        .
        if y > 460:
            y = 460
            speedY = -speedY
        .
        if y < 20:
            y = 20
            speedY = -speedY
        .
    .
    g.background<"#18243A">
    g.fill<"#70D6FF">
    g.circle<x, y, 18>
    if g.key_down<"escape">:
        g.close<>
    .
.

g.run<Frame>
print<"Animation finished.">
