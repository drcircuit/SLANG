// Leksjon: objekttyper, felt som kan endres, lister og typede funksjoner.
// Prøv: legg et kjæledyr til i listen.
obj Kjæledyr:
    navn som tekst
    bein som heltall
    sulten som bool
.

funk GiMat <dyr som Kjæledyr>:
    hvis dyr.sulten:
        print<"Gir mat til", dyr.navn>
        dyr.sulten = usann
    eller:
        print<dyr.navn, "er allerede mett.">
    .
.

var dyrene = Kjæledyr<"Rex", 4, sann>, Kjæledyr<"Kiwi", 2, sann>
for dyr blant dyrene:
    GiMat<dyr>
.
GiMat<dyrene.1>
assert<dyrene.1.sulten er usann>
print<"Første dyr:", dyrene.1.navn, "Bein:", dyrene.1.bein>
