// Et lite spill. Endre tallet i koden og la en venn gjette!
var hemmelig = "7"
var forsok = 0
print<"Jeg tenker på et tall fra 1 til 10. Du har tre forsøk.">

mens forsok < 3:
    var svar = input<"Ditt forslag: ">
    hvis svar er ingen:
        stopp
    .
    forsok++
    hvis svar er hemmelig:
        print<"Riktig! Du brukte", forsok, "forsøk.">
        stopp
    eller hvis forsok er 3:
        print<"Tallet var", hemmelig>
    eller:
        print<"Prøv igjen!">
    .
.
