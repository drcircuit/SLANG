// Exact decimals, an inferred function, and a mutable list.
func Total <prices>:
    var total = 0.0
    for price in prices:
        total = total + price
    .
    ret total
.

var basket = 19.95, 4.50, 0.10
append<basket, 0.20>
print<"The total is", Total<basket>>
print<"Exact decimal addition:", 0.1 + 0.2>
print<"A very large integer:", 999999999999999999999999999999 + 1>

func Counter <>:
    var count = 0
    ret func <>:
        ret ++count
    .
.

var next = Counter<>
print<"A closure remembers:", next<>, next<>, next<>>
