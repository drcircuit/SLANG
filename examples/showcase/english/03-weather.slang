// Lesson: turn text into a typed list, then calculate statistics.
// Try: add a measurement; the average updates automatically.
use strings
use lists
use numbers

var data = "12.4; 12.6; 12.5; 12.3"
var temperatures as list of dec = list<>
for field in strings.split<data, ";">:
    lists.append<temperatures, numbers.parse<field>>
.
var ordered = lists.sort<temperatures>
print<"Measurements:", temperatures>
print<"Lowest:", ordered.1>
print<"Highest:", ordered.<length<ordered>>>
print<"Average:", numbers.format<numbers.mean<temperatures>, 2>>
// Update the expected total if you change the measurements.
assert<numbers.sum<temperatures> is 49.8>
assert<temperatures.1 is 12.4>    // Sorting returned a new list.
