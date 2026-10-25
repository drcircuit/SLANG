// Lesson: randomness, validation, while, continue, and break.
// Try: change the range, and update the accepted choices to match.
use random
use strings
use lists

var secret = random.integer<1, 6>
var accepted = "1", "2", "3", "4", "5", "6"
var attempts = 0
while true:
    var reply = input<"Guess 1-6, or type quit: ">
    if reply is none:
        break
    .
    reply = strings.lower<strings.trim<reply>>
    if reply is "quit":
        break
    .
    if lists.contains<accepted, reply> is false:
        print<"Please type a whole number from 1 to 6.">
        continue
    .
    attempts++
    var guess = int(reply)
    if guess is secret:
        print<"Correct! Attempts:", attempts>
        break
    else:
        if guess < secret:
            print<"Try higher.">
        else:
            print<"Try lower.">
        .
    .
.
print<"Thanks for playing!">
