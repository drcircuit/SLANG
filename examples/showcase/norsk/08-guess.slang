// Leksjon: tilfeldige tall, kontroll av inndata, mens, fortsett og stopp.
// Prøv: endre intervallet og oppdater listen over gyldige svar.
use random som tilfeldig
use strings som tekstverktoy
use lists som lister

var hemmelig = tilfeldig.integer<1, 6>
var gyldige = "1", "2", "3", "4", "5", "6"
var forsøk = 0
mens sann:
    var svar = input<"Gjett 1-6, eller skriv slutt: ">
    hvis svar er ingen:
        stopp
    .
    svar = tekstverktoy.lower<tekstverktoy.trim<svar>>
    hvis svar er "slutt":
        stopp
    .
    hvis lister.contains<gyldige, svar> er usann:
        print<"Skriv et helt tall fra 1 til 6.">
        fortsett
    .
    forsøk++
    var gjett = heltall(svar)
    hvis gjett er hemmelig:
        print<"Riktig! Forsøk:", forsøk>
        stopp
    eller:
        hvis gjett < hemmelig:
            print<"Prøv høyere.">
        eller:
            print<"Prøv lavere.">
        .
    .
.
print<"Takk for spillet!">
