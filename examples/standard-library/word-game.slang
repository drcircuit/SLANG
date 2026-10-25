use strings as s
use random

var words = "blåbær", "snømann", "øy", "skole"
var answer = random.choice<words>
print<"Word:", s.upper<answer>>
print<"Characters:", s.length<answer>>
print<"First character:", s.slice<answer, 1, 1>>
print<"Contains ø:", s.contains<answer, "ø">>
print<"All our words:", s.join<words, ", ">>
