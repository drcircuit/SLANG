use strings
use numbers
use lists

// This text could also come from files.read<"measurements.txt">.
var data = "12.4, 12.6, 12.5, 12.3"
var measurements as list of dec = list<>
for field in strings.split<data, ",">:
    lists.append<measurements, dec(strings.trim<field>)>
.
print<"Measurements:", measurements>
print<"Mean:", numbers.mean<measurements>>
print<"In order:", lists.sort<measurements>>
assert<numbers.sum<measurements> is 49.8>
