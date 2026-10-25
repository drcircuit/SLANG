// Leksjon: en returnert funksjon husker sine egne variabler (en closure).
// Prøv: lag en tredje teller. Deler den poeng med de andre?
funk LagTeller <steglengde som heltall>:
    var poeng = 0
    retur funk <>:
        poeng = poeng + steglengde
        retur poeng
    .
.

var rød = LagTeller<1>
var blå = LagTeller<10>
print<"Rød:", rød<>, rød<>>
print<"Blå:", blå<>, blå<>>
assert<rød<> er 3>
assert<blå<> er 30>

// Funksjoner kan også sendes inn i en annen funksjon.
funk ToGanger <operasjon, verdi>:
    retur operasjon<operasjon<verdi>>
.
var doble = funk <verdi>:
    retur verdi * 2
.
print<"Doble to ganger:", ToGanger<doble, 3>>
