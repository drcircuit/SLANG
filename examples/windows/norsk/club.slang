// Alle sju komponentene i ett skjema. Klikk på lerretet før du skriver i IDE-en.
// Tab bytter felt; piltastene endrer valg; mellomrom krysser av en rute.
use window som vindu
use strings som tekstverktoy

vindu.create<"Start en skoleklubb", 560, 900>
vindu.label<"Velg et prosjekt og bli med i klubben.">
var navn = vindu.input<"Navnet ditt">
var interesser = vindu.checklist<"Interesser", liste<"Programmering", "Tegning", "Musikk">>
var erfaring = vindu.radio_group<"Erfaring", liste<"Nybegynner", "Utforsker">>
var prosjekt = vindu.listbox<"Prosjekt", liste<"Spill", "Værstasjon", "Robot">>
var dag = vindu.combobox<"Møtedag", liste<"Mandag", "Tirsdag", "Onsdag">>
var melding = vindu.label<"Klar når du er det.">

funk BliMed <>:
    var renset = tekstverktoy.trim<navn.value>
    hvis renset er "":
        melding.caption = "Skriv inn navnet ditt først."
        retur
    .
    melding.caption = "Velkommen i klubben, " + renset + "!"
    print<melding.caption>
    print<"Interesser:", tekstverktoy.join<interesser.selected, ", ">>
    print<"Erfaring:", erfaring.value>
    print<"Prosjekt:", prosjekt.value>
    print<"Møtedag:", dag.value>
.

vindu.button<"Bli med i klubben", BliMed>
vindu.button<"Lukk", vindu.close>
vindu.run<>
