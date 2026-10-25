#include "slang/library.hpp"
#include <cstdint>
#include <stdexcept>

namespace slang {
#include "standard_aliases.inc"
const std::map<std::string, std::vector<std::string>>& library_exports() {
    static const std::map<std::string, std::vector<std::string>> exports = {
        {"console", {"print", "input"}},
        {"numbers", {"round", "divide", "abs", "min", "max", "clamp", "floor", "ceil", "pow", "sqrt", "sum", "mean", "parse", "format"}},
        {"strings", {"length", "contains", "find", "slice", "replace", "split", "join", "trim", "lower", "upper"}},
        {"lists", {"length", "append", "remove", "contains", "find", "copy", "slice", "sort", "reverse"}},
        {"random", {"seed", "integer", "choice", "shuffle"}},
        {"time", {"seconds", "sleep"}},
        {"files", {"exists", "read", "write", "append"}},
        {"window", {"create", "label", "input", "button", "checklist", "radio_group", "listbox", "combobox", "run", "close"}},
        {"graphics", {"canvas", "open", "seconds", "present", "run", "close", "fps", "key_down", "background", "fill", "line", "rect", "circle", "sprite", "save"}},
    };
    return exports;
}
namespace {
std::uint32_t decode(const std::string& text, std::size_t& pos) {
    const auto first = static_cast<unsigned char>(text[pos++]);
    if (first < 0x80) return first;
    const std::size_t extra = first >= 0xc2 && first <= 0xdf ? 1 : first >= 0xe0 && first <= 0xef ? 2 : first >= 0xf0 && first <= 0xf4 ? 3 : 0;
    if (!extra || pos + extra > text.size()) throw std::runtime_error("Expected valid UTF-8 text");
    std::uint32_t value = first & (0x7f >> extra);
    for (std::size_t i = 0; i < extra; ++i) {
        const auto next = static_cast<unsigned char>(text[pos++]);
        if ((next & 0xc0) != 0x80) throw std::runtime_error("Expected valid UTF-8 text");
        value = (value << 6) | (next & 0x3f);
    }
    if (value < (extra == 1 ? 0x80u : extra == 2 ? 0x800u : 0x10000u) || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
        throw std::runtime_error("Expected valid UTF-8 text");
    return value;
}
void encode(std::string& text, std::uint32_t value) {
    if (value < 0x80) text += static_cast<char>(value);
    else if (value < 0x800) { text += static_cast<char>(0xc0 | (value >> 6)); text += static_cast<char>(0x80 | (value & 0x3f)); }
    else { text += static_cast<char>(0xe0 | (value >> 12)); text += static_cast<char>(0x80 | ((value >> 6) & 0x3f)); text += static_cast<char>(0x80 | (value & 0x3f)); }
}
bool whitespace(std::uint32_t c) {
    return (c >= 9 && c <= 13) || c == 32 || c == 0x85 || c == 0xa0 || c == 0x1680 ||
        (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 || c == 0x202f || c == 0x205f || c == 0x3000;
}
}
std::vector<std::size_t> text_offsets(const std::string& text) {
    std::vector<std::size_t> offsets;
    for (std::size_t pos = 0; pos < text.size();) { offsets.push_back(pos); (void)decode(text, pos); }
    offsets.push_back(text.size());
    return offsets;
}
std::string text_case(const std::string& text, bool upper) {
    std::string out;
    for (std::size_t pos = 0; pos < text.size();) {
        const auto start = pos;
        auto c = decode(text, pos);
        // Locale-independent basic Latin and Latin-1, including all Nordic letters.
        if (upper && ((c >= 'a' && c <= 'z') || (c >= 0xe0 && c <= 0xf6) || (c >= 0xf8 && c <= 0xfe))) c -= 32;
        else if (!upper && ((c >= 'A' && c <= 'Z') || (c >= 0xc0 && c <= 0xd6) || (c >= 0xd8 && c <= 0xde))) c += 32;
        else if (upper && c == 0xff) c = 0x178;
        else if (!upper && c == 0x178) c = 0xff;
        else if (upper && c == 0xdf) { out += "SS"; continue; }
        else { out.append(text, start, pos - start); continue; }
        encode(out, c);
    }
    return out;
}
std::string text_trim(const std::string& text) {
    std::size_t first = text.size(), last = 0;
    for (std::size_t pos = 0; pos < text.size();) {
        const auto start = pos;
        if (!whitespace(decode(text, pos))) { if (first == text.size()) first = start; last = pos; }
    }
    return last ? text.substr(first, last - first) : std::string{};
}
} // namespace slang
