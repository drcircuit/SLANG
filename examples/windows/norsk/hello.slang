// En etikett, et tekstfelt og en knapp. Prøv å endre hilsenen i funksjonen.
use window som vindu
use strings som tekstverktoy

vindu.create<"Mitt første vindu", 480, 380>
vindu.label<"Skriv navnet ditt og trykk på Hils.">
var navn = vindu.input<"Navnet ditt", "Åse">
var melding = vindu.label<"Klar!">

funk Hils <>:
    var renset = tekstverktoy.trim<navn.value>
    hvis renset er "":
        melding.caption = "Skriv inn navnet ditt."
    eller:
        melding.caption = "Hei, " + renset + "!"
    .
.

vindu.button<"Hils", Hils>
vindu.button<"Lukk", vindu.close>
vindu.run<>
