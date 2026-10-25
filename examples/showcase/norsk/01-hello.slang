// Leksjon: variabler, inndata, vilkår, tekst og gjentakelse.
// Prøv: endre hvor mange ganger programmet hilser.
use strings som tekstverktoy

var navn = input<"Hva heter du? ">
hvis navn er ingen:
    navn = "venn"
eller:
    navn = tekstverktoy.trim<navn>
    hvis navn er "":
        navn = "venn"
    .
.
gjenta 3 ganger:
    print<"Hei, " + navn + "!">
.
