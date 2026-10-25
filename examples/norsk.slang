// Kjør med: slang run examples/norsk.slang
funk Multiply <a, b>:
    retur a * b
.

funk check <condition>:
    assert<condition>
.

funk Add <a, b>:
    retur a + b
.

funk Add <a som heltall, b som tekst>:
    retur tekst(a) + b
.

obj Animal:
    legs som heltall
    furry som bool
    name som tekst
.

var dog = Animal<4, sann, "Rex">
var scores = 1, 2, 3, 4, 5
var details = obj<age = 12, active = sann, name = "Arne">
var positional = obj<12, sann, "Arne">
var empty = liste<>
var single = liste<1>
var missing = ingen
var enormous = 999999999999999999999999999999999999999999
var price = 19.95

funk MakeCounter <>:
    var count = 0
    funk Next <>:
        count++
        retur count
    .
    retur Next
.

var counter = MakeCounter<>
var double = funk <value>:
    retur value * 2
.

for i fra 1 til 5:
    scores.<i>++
.

for score blant scores:
    hvis score > 3 og dog.name er ikke ingen:
        print<score>
    eller:
        print<"Keep trying">
    .
.

gjenta 2 ganger:
    print<counter<>>
.

mens dog.legs > 0:
    dog.legs--
.

check<scores.1 > 0>
Add<2, Multiply<3, 4>>
