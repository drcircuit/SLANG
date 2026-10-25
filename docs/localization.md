# Norsk og engelsk SLANG

SLANG har to sett med navn for samme språk. Norske navn er aliaser for de samme
typene, funksjonene og bibliotekene; de lager ikke en annen kjøremodell.
Engelske og norske navn kan brukes sammen, også i den interaktive REPL-en.

## Språkvalg i IDE-en

Velg **Norsk** eller **English** i språkfeltet på verktøylinjen. Valget lagres
lokalt og gjelder også neste gang IDE-en åpnes. Det endrer appens menyer,
knapper, dialoger og hjelpetekster, og filtrerer forslag til nøkkelord,
standardbiblioteker, funksjoner, vindusegenskaper og fargenavn. Dine egne navn og navn
eksportert av egne biblioteker beholdes. Eksisterende kildekode omskrives ikke.

Noen interne Monaco-editorbeskjeder og tekniske detaljer fra operativsystemet
eller SDL kan fortsatt være engelske. Programmenes egne tekster, fargenavn
som `"blue"` og tastnavn som `"right"` oversettes ikke av språkvalget.

## `#no` og `#en`

Et direktiv på **første linje** velger språket på syntaks- og kjørefeil i filen:

```text
#no
bruk tall
skriv<tall.tolk<"12,50", ",">>
skriv<1 / 0> // Kan ikke dele på null
```

```text
#en
skriv<1 / 0> // Cannot divide by zero
```

Direktivet velger feilmeldinger, ikke hvilke navn programmet får bruke.
Begge eksempelprogrammene er derfor gyldige fram til divisjonen. Direktivet
flytter ikke linjenumre. En kommentar kan stå etter det: `#no // norsk`.
En tom linje eller annen kode foran direktivet er ikke tillatt. Direktiver
senere i filen og ukjente direktiver avvises. En innledende UTF-8-BOM er tillatt.

Uten direktiv brukes IDE-ens språkvalg. I den native CLI-en er standarden
engelsk; `--locale` gir en annen standard for filer uten direktiv:

```text
slang run --locale no program.slang
slang check --json --locale no program.slang
slang ast --locale en program.slang
```

Et direktiv i filen overstyrer denne standarden. En feil inne i et importert
bibliotek bruker direktivet i bibliotekfilen. Uten eget direktiv bruker også
biblioteket IDE-/CLI-standarden. Den bruker ikke automatisk hovedfilens direktiv.

Programutskrift og meldingen som eleven selv gir til `feil` / `error`, beholdes
ordrett. `sjekk` / `assert` oversetter innledningen, men beholder elevens melding.
Verdier gjengis fortsatt med faste navn som `true`, `false`, `none`, `int` og
`dec`; `type<verdi>` gir de kanoniske typenavnene. Vanlige syntaks- og kjørefeil
er oversatt. Meldinger uten en oversettelse vises med «Teknisk feil:» foran den
opprinnelige forklaringen, slik at nyttige detaljer ikke forsvinner.

Den separate `slang repl` godtar begge språk samtidig, uten språkmodus eller
behov for direktiver. Den har foreløpig engelske ledetekster og hjelp.
`--locale` er et valg for filkommandoene, ikke REPL-en. JavaScript-verktøyet
`src/cli.js` er en utviklerreferanse for syntaks; det forstår direktivene, men
har fortsatt engelske feilmeldinger. Den distribuerte CLI-en bruker native-tolken.

## Typer og grunnleggende navn

| Engelsk | Norsk |
| --- | --- |
| `int` | `heltall` |
| `dec` | `desimal` |
| `text` | `tekst` |
| `bool` | `logisk` |
| `list` | `liste` |
| `obj` | `objekt` |
| `true` / `false` / `none` | `sann` / `usann` / `ingen` |
| `func` / `ret` | `funk` / `retur` |
| `use` / `lib` / `public` | `bruk` / `bibliotek` / `offentlig` |
| `if` / `else` | `hvis` / `eller` |
| `and` / `or` / `not` / `is` | `og` / `eller` / `ikke` / `er` |
| `while` | `mens` |
| `repeat` / `times` | `gjenta` / `ganger` |
| `from` / `to` / `step` / `in` | `fra` / `til` / `steg` / `blant` |
| `break` / `continue` | `stopp` / `fortsett` |
| `as` / `of` | `som` / `av` |

`var`, `for` og funksjonen `type` har samme navn i begge utgavene.
Konteksten skiller `eller` som alternativ gren fra `eller` som logisk operator.
Lister kan lages med `liste<>`, og objekter med `objekt<...>` eller en navngitt
objekttype. Vanlige egenskapsnavn i elevenes egne objekter oversettes aldri.

## Eksempel med vindu

```text
#no
bruk vindu
vindu.opprett<"Hei-appen", 480, 360>
var navn = vindu.tekstfelt<"Navn", "Åse">
var melding = vindu.etikett<"Klar!">
funk Hils <>:
    melding.tekst = "Hei, " + navn.verdi
.
vindu.knapp<"Hils", Hils>
vindu.kjør<>
```

Norske og engelske vindusegenskaper viser til samme verdi: en endring gjennom
`.tekst` vises også gjennom `.caption`. Aliasene gjelder bare bibliotekets
vinduskontroller. De legger ikke til ekstra indeksplasser i objektet.

<!-- GENERATED ALIAS TABLES -->

## Innebygde funksjoner

| Engelsk | Norsk |
| --- | --- |
| `print` | `skriv` |
| `input` | `les` |
| `length` | `lengde` |
| `append` | `legg_til` |
| `remove` | `fjern` |
| `round` | `avrund` |
| `divide` | `del` |
| `assert` | `sjekk` |
| `error` | `feil` |

## Vindusegenskaper

| Engelsk | Norsk |
| --- | --- |
| `caption` | `tekst` |
| `enabled` | `aktiv` |
| `value` | `verdi` |
| `items` | `elementer` |
| `selected` | `valgte` |

## Fargenavn

Norske og engelske navn kan brukes i `grafikk.fyll` / `graphics.fill` og `grafikk.bakgrunn` / `graphics.background`. Store og små bokstaver virker likt, også `RØD`, `BLÅ` og `GRÅ`. `grey` er også et engelsk navn for `grå`; `cyan` og `magenta` er like på begge språk. Hex-koder som `"#AABBCC"` fungerer som før. IDE-en foreslår fargenavn på valgt språk uten å endre eksisterende tekst i programmet.

| Engelsk | Norsk |
| --- | --- |
| `black` | `svart` |
| `white` | `hvit` |
| `red` | `rød` |
| `green` | `grønn` |
| `blue` | `blå` |
| `yellow` | `gul` |
| `orange` | `oransje` |
| `purple` | `lilla` |
| `pink` | `rosa` |
| `gray` | `grå` |
| `cyan` | `cyan` |
| `magenta` | `magenta` |

## Standardbiblioteker

| Engelsk | Norsk |
| --- | --- |
| `console` | `konsoll` |
| `numbers` | `tall` |
| `strings` | `tekster` |
| `lists` | `lister` |
| `random` | `tilfeldig` |
| `time` | `tid` |
| `files` | `filer` |
| `window` | `vindu` |
| `graphics` | `grafikk` |

### konsoll / console

| Engelsk | Norsk |
| --- | --- |
| `print` | `skriv` |
| `input` | `les` |

### tall / numbers

| Engelsk | Norsk |
| --- | --- |
| `round` | `avrund` |
| `divide` | `del` |
| `abs` | `absolutt` |
| `min` | `minste` |
| `max` | `største` |
| `clamp` | `begrens` |
| `floor` | `rund_ned` |
| `ceil` | `rund_opp` |
| `pow` | `potens` |
| `sqrt` | `kvadratrot` |
| `sum` | `sum` |
| `mean` | `gjennomsnitt` |
| `parse` | `tolk` |
| `format` | `formater` |

### tekster / strings

| Engelsk | Norsk |
| --- | --- |
| `length` | `lengde` |
| `contains` | `inneholder` |
| `find` | `finn` |
| `slice` | `utsnitt` |
| `replace` | `erstatt` |
| `split` | `del_opp` |
| `join` | `sett_sammen` |
| `trim` | `trim` |
| `lower` | `små_bokstaver` |
| `upper` | `store_bokstaver` |

### lister / lists

| Engelsk | Norsk |
| --- | --- |
| `length` | `lengde` |
| `append` | `legg_til` |
| `remove` | `fjern` |
| `contains` | `inneholder` |
| `find` | `finn` |
| `copy` | `kopier` |
| `slice` | `utsnitt` |
| `sort` | `sorter` |
| `reverse` | `snu` |

### tilfeldig / random

| Engelsk | Norsk |
| --- | --- |
| `seed` | `frø` |
| `integer` | `heltall` |
| `choice` | `velg` |
| `shuffle` | `bland` |

### tid / time

| Engelsk | Norsk |
| --- | --- |
| `seconds` | `sekunder` |
| `sleep` | `vent` |

### filer / files

| Engelsk | Norsk |
| --- | --- |
| `exists` | `finnes` |
| `read` | `les` |
| `write` | `skriv` |
| `append` | `legg_til` |

### vindu / window

| Engelsk | Norsk |
| --- | --- |
| `create` | `opprett` |
| `label` | `etikett` |
| `input` | `tekstfelt` |
| `button` | `knapp` |
| `checklist` | `sjekkliste` |
| `radio_group` | `radiogruppe` |
| `listbox` | `listeboks` |
| `combobox` | `komboboks` |
| `run` | `kjør` |
| `close` | `lukk` |

### grafikk / graphics

| Engelsk | Norsk |
| --- | --- |
| `canvas` | `lerret` |
| `open` | `åpen` |
| `seconds` | `sekunder` |
| `present` | `vis` |
| `run` | `kjør` |
| `close` | `lukk` |
| `fps` | `bilder_per_sekund` |
| `key_down` | `tast_nede` |
| `background` | `bakgrunn` |
| `fill` | `fyll` |
| `line` | `linje` |
| `rect` | `rektangel` |
| `circle` | `sirkel` |
| `sprite` | `bilde` |
| `save` | `lagre` |

Tabellene genereres fra `src/standard-aliases.json`, som også brukes av IDE-en og native-bygget. Oppdater med `node scripts/update-localization-doc.mjs`.
