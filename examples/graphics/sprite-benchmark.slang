use graphics as g
use numbers

// A reproducible five-second workload, not a guarantee for every school PC.
// 120 moving sprites at 960 x 540, with arithmetic and loops in SLANG.
g.canvas<960, 540, "SLANG sprite benchmark: 300 frames">
var frames = 0
var total as dec = 0
var slow = 0
var worst as dec = 0
var offset as dec = 0

while g.open<>:
    var seconds = g.seconds<>
    if frames > 0:
        total = total + seconds
        if seconds > worst:
            worst = seconds
        .
        if seconds > 0.020:
            slow++
        .
    .
    offset = offset + seconds * 50
    if offset > 25:
        offset = offset - 25
    .
    g.background<"#18243A">
    for row from 0 to 7:
        for column from 0 to 14:
            g.sprite<"ship.png", column * 62 + offset, row * 64, 32, 32>
        .
    .
    g.present<>
    frames++
    if frames is 300:
        g.close<>
    .
.
if frames > 1:
    print<"Average FPS:", numbers.round<(frames - 1) / total, 2>>
    print<"Worst frame (ms):", numbers.round<worst * 1000, 2>>
    print<"Frames over 20 ms:", slow>
.
