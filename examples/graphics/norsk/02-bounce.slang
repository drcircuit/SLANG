// CC0. Leksjon: animasjon med en tilbakeringingsfunksjon og medgåtte sekunder.
// Hold mellomrom for å fryse ballen; Escape lukker. Prøv en annen fart.
use graphics som g

g.canvas<640, 480, "Sprettball | Hold mellomrom for å fryse | Escape lukker">
var x som desimal = 70
var y som desimal = 90
var fartX = 180
var fartY = 130

funk Bilde <sekunder som desimal>:
    hvis ikke g.key_down<"space">:
        x = x + fartX * sekunder
        y = y + fartY * sekunder
        hvis x > 620:
            x = 620
            fartX = -fartX
        .
        hvis x < 20:
            x = 20
            fartX = -fartX
        .
        hvis y > 460:
            y = 460
            fartY = -fartY
        .
        hvis y < 20:
            y = 20
            fartY = -fartY
        .
    .
    g.background<"#18243A">
    g.fill<"#70D6FF">
    g.circle<x, y, 18>
    hvis g.key_down<"escape">:
        g.close<>
    .
.

g.run<Bilde>
print<"Animasjonen er ferdig.">
