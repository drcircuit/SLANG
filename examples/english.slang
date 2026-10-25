// Run with: slang run examples/english.slang
func Multiply <a, b>:
    ret a * b
.

func check <condition>:
    assert<condition>
.

func Add <a, b>:
    ret a + b
.

func Add <a as int, b as text>:
    ret text(a) + b
.

obj Animal:
    legs as int
    furry as bool
    name as text
.

var dog = Animal<4, true, "Rex">
var scores = 1, 2, 3, 4, 5
var details = obj<age = 12, active = true, name = "Arne">
var positional = obj<12, true, "Arne">
var empty = list<>
var single = list<1>
var missing = none
var enormous = 999999999999999999999999999999999999999999
var price = 19.95

func MakeCounter <>:
    var count = 0
    func Next <>:
        count++
        ret count
    .
    ret Next
.

var counter = MakeCounter<>
var double = func <value>:
    ret value * 2
.

for i from 1 to 5:
    scores.<i>++
.

for score in scores:
    if score > 3 and dog.name is not none:
        print<score>
    else:
        print<"Keep trying">
    .
.

repeat 2 times:
    print<counter<>>
.

while dog.legs > 0:
    dog.legs--
.

check<scores.1 > 0>
Add<2, Multiply<3, 4>>
