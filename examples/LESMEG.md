# Lær SLANG med små programmer

[English guide](README.md)

Her finner du **11 leksjoner på både norsk og engelsk**. De to versjonene lærer bort de samme ideene. I de norske filene er også kommentarer, variabelnavn, spørsmål og utskrift oversatt. Filnavnene er like, slik at du lett kan sammenligne versjonene.

I IDE-en åpner du **showcase → norsk** i filoversikten. Velg en nummerert fil og trykk **Run** (F5). Skriv svar i konsollens inndatafelt og trykk Enter. **Stop** avslutter programmet. Med **Debug** og **Step** kan du følge variablene trinn for trinn. Begynn gjerne med de fire første leksjonene, og prøv deretter grafikk eller fortsett med funksjoner og biblioteker.

Fra en terminal bruker du `slang run sti/til/eksempel.slang`. Hvis du har bygget kildekoden på Windows:

```powershell
.\build\native-ninja\slang.exe run .\examples\showcase\norsk\01-hello.slang
.\build\native-ninja\slang.exe run .\examples\graphics\norsk\03-coins.slang
```

## Konsollprogrammer

| Leksjon | Norsk | Engelsk | Dette lærer du |
| --- | --- | --- | --- |
| 1. Hei! | [Åpne](showcase/norsk/01-hello.slang) | [Open](showcase/english/01-hello.slang) | Variabler, tekst, inndata, vilkår og gjentakelse |
| 2. Lommepenger | [Åpne](showcase/norsk/02-pocket-money.slang) | [Open](showcase/english/02-pocket-money.slang) | Nøyaktige desimaltall, priser, funksjoner, løkker og store heltall |
| 3. Værmålinger | [Åpne](showcase/norsk/03-weather.slang) | [Open](showcase/english/03-weather.slang) | Dele tekst, typede lister, sortering, indekser og statistikk |
| 4. Kjæledyr | [Åpne](showcase/norsk/04-pets.slang) | [Open](showcase/english/04-pets.slang) | Objekttyper, felt som kan endres og typede funksjoner |
| 5. Tellere | [Åpne](showcase/norsk/05-counters.slang) | [Open](showcase/english/05-counters.slang) | Funksjoner som husker egne variabler, og funksjoner som argumenter |
| 6. Butikkbibliotek | [Åpne](showcase/norsk/06-library.slang) | [Open](showcase/english/06-library.slang) | Import, offentlige typer og funksjoner, private hjelpere |
| 7. Dagbok | [Åpne](showcase/norsk/07-notebook.slang) | [Open](showcase/english/07-notebook.slang) | Lagre og lese UTF-8-tekst, bevare data mellom kjøringer |
| 8. Gjettelek | [Åpne](showcase/norsk/08-guess.slang) | [Open](showcase/english/08-guess.slang) | Tilfeldige tall, kontroll av inndata, mens, fortsett og stopp |

Dagboken oppretter eller legger til tekst i **my-notebook.txt ved siden av kildefilen**. Bruk en mappe du kan skrive til. Kjør programmet to ganger og se at den første teksten fortsatt finnes. Butikkleksjonen bruker biblioteket `helpers/shop.slang`; ta med denne mappen hvis du kopierer leksjonen. Hjelpefilene er biblioteker, ikke egne leksjoner.

Gjetteleken godtar heltall fra 1 til 6. Andre svar gir en forklaring. Skriv `slutt` for å avslutte, eller `quit` i den engelske versjonen. Slutt på inndata (EOF) avslutter også. Du kan bruke `æ`, `ø` og `å` i variabelnavn. Desimaltall i kildekoden skrives med punktum, som `var øre = 0.01`. Komma i inndata og utskrift støttes gjennom `numbers.parse` og `numbers.format`. Bibliotekfunksjoner og innebygde funksjoner som `print`, `input` og `assert` har foreløpig felles navn i begge språkversjonene.

## Grafikk og spill

Åpne **graphics → norsk** i filoversikten. I IDE-en vises grafikken på lerretet; fra terminalen åpnes et eget vindu.

| Leksjon | Norsk | Engelsk | Kontroller og ideer |
| --- | --- | --- | --- |
| Søylediagram | [Åpne](graphics/norsk/01-chart.slang) | [Open](graphics/english/01-chart.slang) | Automatisk lerret, lister, koordinater og farger; konsollen forklarer søylene |
| Sprettball | [Åpne](graphics/norsk/02-bounce.slang) | [Open](graphics/english/02-bounce.slang) | En funksjon kalles for hvert bilde; hold mellomrom for å fryse ballen, Escape lukker |
| Samle mynter | [Åpne](graphics/norsk/03-coins.slang) | [Open](graphics/english/03-coins.slang) | Vanlig mens-løkke, bilde, piltaster, kollisjon og tilfeldige mål |

**Klikk på lerretet før du bruker tastene i IDE-en.** I myntspillet flytter du skipet med piltastene og samler fem gule mynter. Poeng og seiersmelding vises i konsollen. Escape avslutter. Behold `ship.png` i grafikkmappen over språkmappene hvis du kopierer spillet. Animasjonen og spillet bruker bibliotekets standardmål på 60 bilder per sekund og medgått tid til bevegelse; faktisk ytelse avhenger av maskinen.

Søylediagrammet tegnes én gang. Fra terminalen avslutter du ved å lukke vinduet. I IDE-en avsluttes programmet, men bildet blir stående. Prøv å endre én verdi som foreslått i `Prøv`-kommentaren øverst i hver fil. Gjett hva som vil skje før du kjører programmet igjen. Noen `assert`-linjer kontrollerer de opprinnelige eksempelverdiene; oppdater forventningen når du endrer data med vilje.

## Mer hjelp og lisenser

Prøv også [en enkel hilsen i et vindu](windows/norsk/hello.slang) og [skjemaet for en skoleklubb](windows/norsk/club.slang). Klubbskjemaet viser etiketter, tekstfelt, knapper, avkrysningslister, radiogrupper, listebokser og nedtrekkslister. Tilsvarende engelske eksempler ligger i `windows/english/`. Se [veiledningen for vinduer](../docs/windows.md).

- [Standardbibliotekene](../docs/standard-library.md), [språkreglene](../docs/runtime.md) og [grafikkbiblioteket](../docs/graphics.md) er dokumentert på engelsk.
- Konsollprogrammene og hjelpebibliotekene bruker prosjektets [MIT-lisens](../LICENSE).
- De originale grafikkeksemplene og bildene under `graphics/` bruker [CC0](graphics/README.md).

Utviklere kan kjøre `npm run examples:test` etter å ha bygget tolken. Testene kontrollerer begge språkversjonene, inndata, filer, tegning og spillkontroller. Filskriving under test skjer i en egen midlertidig mappe.
