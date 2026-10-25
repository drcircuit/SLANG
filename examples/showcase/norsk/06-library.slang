// Leksjon: importer offentlige typer og funksjoner; hjelpere forblir private.
// Prøv: legg til en tredje vare. Åpne helpers/shop.slang og se hvordan det virker.
use "helpers/shop.slang" som butikk
use numbers som matte

var kurv som liste av butikk.Vare = butikk.Vare<"Eple", 5.50>, butikk.Vare<"Brød", 24.40>
print<"Sum:", matte.format<butikk.Sum<kurv>, 2, ",">, "kr">
// Oppdater den forventede summen hvis du endrer kurven.
assert<butikk.Sum<kurv> er 29.90>
// butikk.Kontroller er privat og kan ikke kalles herfra.
