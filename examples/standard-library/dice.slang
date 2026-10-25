use random
use numbers
use lists

random.seed<42>                 // Remove this line for a different experiment each run.
var rolls = list<>
repeat 20 times:
    lists.append<rolls, random.integer<1, 6>>
.
print<"Rolls:", rolls>
print<"Sorted:", lists.sort<rolls>>
print<"Total:", numbers.sum<rolls>>
print<"Average:", numbers.round<numbers.mean<rolls>, 2>>
