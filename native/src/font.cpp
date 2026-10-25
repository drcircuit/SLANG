#include "slang/font.hpp"
#include <stdexcept>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"
namespace slang {
namespace {
// Only this trusted, embedded font is parsed. No student-supplied font files.
const unsigned char bytes[] = {
#include "font_data.inc"
};
const stbtt_fontinfo& font() {
    static const auto info = [] {
        stbtt_fontinfo value{};
        if (!stbtt_InitFont(&value, bytes, stbtt_GetFontOffsetForIndex(bytes, 0))) throw std::runtime_error("Cannot initialize the bundled font");
        return value;
    }();
    return info;
}
float scale() { return stbtt_ScaleForPixelHeight(&font(), 24); }
}
float font_advance(int codepoint) {
    int advance = 0; stbtt_GetCodepointHMetrics(&font(), codepoint, &advance, nullptr); return advance * scale();
}
Glyph font_glyph(int codepoint) {
    Glyph result; result.advance = font_advance(codepoint);
    auto* pixels = stbtt_GetCodepointBitmap(&font(), scale(), scale(), codepoint, &result.width, &result.height, &result.x, &result.y);
    if (pixels) { result.alpha.assign(pixels, pixels + result.width * result.height); stbtt_FreeBitmap(pixels, nullptr); }
    return result;
}
} // namespace slang
