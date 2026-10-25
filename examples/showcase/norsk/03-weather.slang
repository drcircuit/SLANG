// Leksjon: gjør tekst om til en typet liste og beregn statistikk.
// Prøv: legg til en måling; gjennomsnittet oppdateres automatisk.
use strings som tekstverktoy
use lists som lister
use numbers som matte

var data = "12,4; 12,6; 12,5; 12,3"
var temperaturer som liste av desimal = liste<>
for felt blant tekstverktoy.split<data, ";">:
    lister.append<temperaturer, matte.parse<felt, ",">>
.
var sortert = lister.sort<temperaturer>
print<"Målinger:", temperaturer>
print<"Lavest:", sortert.1>
print<"Høyest:", sortert.<length<sortert>>>
print<"Gjennomsnitt:", matte.format<matte.mean<temperaturer>, 2, ",">>
// Oppdater den forventede summen hvis du endrer målingene.
assert<matte.sum<temperaturer> er 49.8>
assert<temperaturer.1 er 12.4>    // Sorteringen ga oss en ny liste.
