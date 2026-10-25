// Lesson: variables, input, conditions, text, and a repeat loop.
// Try: change the number of greetings.
use strings

var name = input<"What is your name? ">
if name is none:
    name = "friend"
else:
    name = strings.trim<name>
    if name is "":
        name = "friend"
    .
.
repeat 3 times:
    print<"Hello, " + name + "!">
.
