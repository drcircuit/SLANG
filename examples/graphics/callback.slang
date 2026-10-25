use graphics as g

g.canvas<640, 480, "A bouncing ball: callback version">
var x as dec = 40
var speed = 160

func Frame <seconds as dec>:
    x = x + speed * seconds
    if x > 610:
        x = 610
        speed = -160
    .
    if x < 30:
        x = 30
        speed = 160
    .
    g.background<"#18243A">
    g.fill<"#FFD166">
    g.circle<x, 240, 25>
    if g.key_down<"escape">:
        g.close<>
    .
.

g.run<Frame>
print<"Animation finished.">
