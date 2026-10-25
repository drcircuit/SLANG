// Lesson: import public types/functions; private helpers stay in the library.
// Try: add a third item. Open helpers/shop.slang to see the implementation.
use "helpers/shop.slang" as shop
use numbers

var basket as list of shop.Item = shop.Item<"Apple", 5.50>, shop.Item<"Bread", 24.40>
print<"Total:", numbers.format<shop.Total<basket>, 2>, "kr">
// Update the expected total if you change the basket.
assert<shop.Total<basket> is 29.90>
// shop.Validate is private and cannot be called here.
