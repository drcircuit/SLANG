// Leksjon: lagre UTF-8-tekst og finn den igjen neste gang programmet kjører.
// Dette oppretter eller legger til tekst i my-notebook.txt ved siden av kildefilen.
// Prøv: kjør to ganger og åpne deretter my-notebook.txt i filoversikten.
use files som filer

var filsti = "my-notebook.txt"
hvis filer.exists<filsti> er usann:
    filer.write<filsti, "Min programmeringsdagbok\n">
.
filer.append<filsti, "I dag lærte jeg å lagre tekst med æ, ø og å.\n">
print<filer.read<filsti>>
