// CC0. Leksjon: tegn et søylediagram fra en liste. Lerretet opprettes automatisk.
// Prøv: endre stemmene eller fargene. Lukk vinduet når du er ferdig.
use graphics som g

var navn = "Katter", "Hunder", "Fugler", "Fisk", "Kaniner"
var stemmer = 6, 9, 3, 5, 7
var farger = "#FF70A6", "#70D6FF", "#FFD670", "#A8E6A1", "#BE95FF"

g.background<"#18243A">
g.fill<"#AABBD0">
g.line<50, 50, 50, 400>
g.line<50, 400, 600, 400>
for i fra 1 til length<stemmer>:
    var høyde = stemmer.<i> * 32
    var x = 75 + (i - 1) * 105
    g.fill<farger.<i>>
    g.rect<x, 400 - høyde, 65, høyde>
    print<navn.<i>, stemmer.<i>>
.
print<"Våre favorittdyr — søylene følger rekkefølgen i konsollen.">
