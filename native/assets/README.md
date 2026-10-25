# Embedded window font

`NotoSans.ttf` is the unmodified Noto Sans variable font from Google Fonts,
revision `e7db6ca603fd4862010383ff17ffd6cc4ffac1c0`, path
`ofl/notosans/NotoSans[wdth,wght].ttf`.

Source: https://github.com/google/fonts/tree/e7db6ca603fd4862010383ff17ffd6cc4ffac1c0/ofl/notosans

SHA-256: `bfb7bb691513f12e734dc346c03a03f784912432d7e3fa8e56efcf906fe86b3d`

The font uses the SIL Open Font License in `OFL.txt`. CMake embeds it into the
native interpreter so installed fonts and extra runtime files are unnecessary.
The renderer uses its default regular outline. Supported glyphs include Latin,
Greek and Cyrillic; this first window library does not implement text shaping
for complex scripts or bidirectional layout.
