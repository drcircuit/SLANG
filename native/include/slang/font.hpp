#pragma once
#include <cstdint>
#include <vector>
namespace slang {
struct Glyph { int width = 0, height = 0, x = 0, y = 0; float advance = 0; std::vector<std::uint8_t> alpha; };
Glyph font_glyph(int codepoint);
float font_advance(int codepoint);
} // namespace slang
