// Lesson: exact decimals, functions, ranges, and formatted output.
// Try: change the weekly saving or number of weeks.
use numbers as maths

func Money <amount as dec>:
    var formatted = maths.format<amount, 2>
    ret formatted + " kr"
.

var savings = 0.00
var weekly = 12.50
for week from 1 to 4:
    savings = savings + weekly
    print<"Week", week, Money<savings>>
.

var price = 39.90
if savings >= price:
    print<"You can buy it! Money left:", Money<savings - price>>
else:
    print<"Keep saving!">
.
// Update this expected answer if you change the saving or price.
assert<savings - price is 10.10>
assert<0.1 + 0.2 is 0.3>
print<"A large integer:", maths.pow<2, 100>>
