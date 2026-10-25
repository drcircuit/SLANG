# Licensing and third-party notices

SLANG's original interpreter, IDE, tooling, and documentation are licensed
under MIT; see [LICENSE](LICENSE). The original programs and artwork under
`examples/graphics/` and the IDE's built-in drawing example use CC0 1.0
Universal instead; see [the graphics license](examples/graphics/LICENSE).
Separately distributed classroom materials carry their own license notices.

Dependencies retain their own licenses. The project's MIT license and CC0
dedication do not replace those licenses. Release archives carry their notices:

| Component | License / notices |
| --- | --- |
| Electron | MIT; Electron's `LICENSE` in the IDE distribution |
| Chromium and Electron dependencies | `LICENSES.chromium.html` in the IDE distribution |
| Monaco Editor | MIT and `ThirdPartyNotices.txt`; copied into `licenses/` |
| DOMPurify | Apache-2.0 OR MPL-2.0; its LICENSE is copied into `licenses/` |
| SDL3 | zlib; copied into `licenses/` |
| Noto Sans (embedded window font) | SIL Open Font License 1.1; copied into `licenses/` |
| stb_truetype (embedded font rendering) | MIT license option; copied into `licenses/` |
| SDL's embedded HIDAPI, yuv2rgb, stb_image, miniz | Original notices copied into `licenses/` |

Build-only packages are not shipped as application dependencies. Their license
files remain in the installed development packages. Do not remove third-party
notices when redistributing the packaged application.
