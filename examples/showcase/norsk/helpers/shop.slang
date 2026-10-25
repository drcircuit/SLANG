// Importer biblioteket fra 06-library.slang; dette er ikke en egen leksjon.
lib butikk:
    funk Kontroller <pris>:
        assert<pris >= 0, "En pris kan ikke være negativ">
    .

    public obj Vare:
        navn som tekst
        pris som desimal
    .

    public funk Sum <varer>:
        var sum = 0.00
        for vare blant varer:
            Kontroller<vare.pris>
            sum = sum + vare.pris
        .
        retur sum
    .
.
