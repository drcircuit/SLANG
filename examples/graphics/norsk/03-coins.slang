// CC0. Leksjon: et lite spill med en vanlig mens-løkke, bilder og kollisjon.
// Piltastene flytter. Samle 5 mynter. Escape lukker. Klikk først på lerretet i IDE-en.
// Prøv: endre målet, skipets fart eller myntfargen.
use graphics som g
use numbers som matte
use random som tilfeldig

g.canvas<640, 480, "Samle 5 mynter | Piltastene flytter | Escape lukker">
var x som desimal = 60
var y som desimal = 240
var mynt = obj<x = 320, y = 240>
var poeng = 0
var mål = 5
print<"Samle de gule myntene! Poengene vises her.">

mens g.open<>:
    var avstand = 240 * matte.min<g.seconds<>, 0.05>
    hvis g.key_down<"right">:
        x = x + avstand
    .
    hvis g.key_down<"left">:
        x = x - avstand
    .
    hvis g.key_down<"down">:
        y = y + avstand
    .
    hvis g.key_down<"up">:
        y = y - avstand
    .
    x = matte.clamp<x, 16, 624>
    y = matte.clamp<y, 16, 464>

    var dx = x - mynt.x
    var dy = y - mynt.y
    hvis dx * dx + dy * dy < 24 * 24:
        poeng++
        print<"Mynter:", poeng, "/", mål>
        hvis poeng er mål:
            print<"Du vant!">
            g.close<>
            stopp
        .
        mynt.x = tilfeldig.integer<80, 560>
        mynt.y = tilfeldig.integer<80, 400>
    .
    g.background<"#18243A">
    g.fill<"#FFD670">
    g.circle<mynt.x, mynt.y, 8>
    g.sprite<"../ship.png", x - 16, y - 16, 32, 32>
    hvis g.key_down<"escape">:
        g.close<>
        stopp
    .
    g.present<>
.
print<"Spillet er ferdig.">
