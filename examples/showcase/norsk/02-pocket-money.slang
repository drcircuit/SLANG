// Leksjon: nøyaktige desimaltall, funksjoner, løkker og formatering.
// Prøv: endre ukebeløpet eller antall uker.
use numbers som matte

funk Kroner <beløp som desimal>:
    var formatert = matte.format<beløp, 2, ",">
    retur formatert + " kr"
.

var sparepenger = 0.00
var ukebeløp = 12.50
for uke fra 1 til 4:
    sparepenger = sparepenger + ukebeløp
    print<"Uke", uke, Kroner<sparepenger>>
.

var pris = 39.90
hvis sparepenger >= pris:
    print<"Du har råd! Penger igjen:", Kroner<sparepenger - pris>>
eller:
    print<"Fortsett å spare!">
.
// Oppdater det forventede svaret hvis du endrer sparingen eller prisen.
assert<sparepenger - pris er 10.10>
assert<0.1 + 0.2 er 0.3>
print<"Et stort heltall:", matte.pow<2, 100>>
