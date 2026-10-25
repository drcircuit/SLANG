// Lesson: a returned function remembers its own variables (a closure).
// Try: create a third counter. Does it share the others' score?
func MakeCounter <increment as int>:
    var score = 0
    ret func <>:
        score = score + increment
        ret score
    .
.

var red = MakeCounter<1>
var blue = MakeCounter<10>
print<"Red:", red<>, red<>>
print<"Blue:", blue<>, blue<>>
assert<red<> is 3>
assert<blue<> is 30>

// Functions can also be passed into another function.
func Twice <operation, value>:
    ret operation<operation<value>>
.
var double = func <value>:
    ret value * 2
.
print<"Double twice:", Twice<double, 3>>
