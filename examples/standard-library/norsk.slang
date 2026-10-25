use random som tilfeldig
use numbers som matte
use strings som tekstverktoy

var kast = list<>
gjenta 10 ganger:
    append<kast, tilfeldig.integer<1, 6>>
.
print<tekstverktoy.upper<"terningkast">, kast>
print<"Gjennomsnitt:", matte.mean<kast>>
var øre = 0.01
var pris = matte.parse<"12,50", ",">
print<"Pris:", matte.format<pris + øre, 2, ",">, "kr">
