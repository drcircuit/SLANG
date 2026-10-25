#include "slang/lexer.hpp"
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <utility>

namespace slang {
namespace {
// Both vocabularies resolve to the same parser tokens.
using namespace std::string_view_literals;
constexpr std::pair<std::string_view, std::string_view> aliases[] = {
    {"lib", "lib"}, {"bibliotek", "lib"}, {"public", "public"}, {"offentlig", "public"}, {"use", "use"}, {"bruk", "use"},
    {"var", "var"}, {"func", "func"}, {"funk", "func"}, {"ret", "ret"}, {"retur", "ret"},
    {"obj", "obj"}, {"objekt", "obj"}, {"if", "if"}, {"hvis", "if"}, {"else", "else"}, {"eller", "else_or"},
    {"while", "while"}, {"mens", "while"}, {"for", "for"}, {"in", "in"}, {"blant", "in"},
    {"from", "from"}, {"fra", "from"}, {"to", "to"}, {"til", "to"}, {"step", "step"}, {"steg", "step"},
    {"repeat", "repeat"}, {"gjenta", "repeat"}, {"times", "times"}, {"ganger", "times"},
    {"break", "break"}, {"stopp", "break"}, {"continue", "continue"}, {"fortsett", "continue"},
    {"as", "as"}, {"som", "as"}, {"of", "of"}, {"av", "of"}, {"and", "and"}, {"og", "and"},
    {"or", "or"}, {"not", "not"}, {"ikke", "not"}, {"is", "is"}, {"er", "is"},
    {"true", "true"}, {"sann", "true"}, {"false", "false"}, {"usann", "false"},
    {"none", "none"}, {"ingen", "none"}, {"int", "int"}, {"heltall", "int"},
    {"dec", "dec"}, {"desimal", "dec"}, {"text", "text"}, {"tekst", "text"},
    {"bool", "bool"}, {"logisk", "bool"}, {"list", "list"}, {"liste", "list"}
};

struct UnicodeRange { char32_t first; char32_t last; };
#include "unicode_ranges.inc"

template<std::size_t N>
bool in_ranges(char32_t cp, const UnicodeRange (&ranges)[N]) {
    const auto found = std::lower_bound(std::begin(ranges), std::end(ranges), cp,
        [](const UnicodeRange& range, char32_t value) { return range.last < value; });
    return found != std::end(ranges) && found->first <= cp;
}
bool digit(char32_t cp) { return cp >= U'0' && cp <= U'9'; }
bool letter(char32_t cp) {
    return cp == U'_' || (cp >= U'a' && cp <= U'z') || (cp >= U'A' && cp <= U'Z') ||
        (cp >= 128 && in_ranges(cp, letter_ranges));
}
bool continuation(char32_t cp) {
    return letter(cp) || digit(cp) || (cp >= 128 && in_ranges(cp, continuation_ranges));
}
void append_utf8(std::string& text, char32_t cp) {
    if (cp < 0x80) text += static_cast<char>(cp);
    else if (cp < 0x800) {
        text += static_cast<char>(0xc0 | (cp >> 6));
        text += static_cast<char>(0x80 | (cp & 0x3f));
    } else if (cp < 0x10000) {
        text += static_cast<char>(0xe0 | (cp >> 12));
        text += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
        text += static_cast<char>(0x80 | (cp & 0x3f));
    } else {
        text += static_cast<char>(0xf0 | (cp >> 18));
        text += static_cast<char>(0x80 | ((cp >> 12) & 0x3f));
        text += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
        text += static_cast<char>(0x80 | (cp & 0x3f));
    }
}

class Lexer {
public:
    explicit Lexer(std::string_view source) : source_(source) {}
    std::vector<Token> run();
private:
    std::string_view source_;
    Position at_;
    std::vector<Token> tokens_;
    bool done() const { return at_.offset == source_.size(); }
    [[noreturn]] void fail(std::string message, Position start) const {
        throw Diagnostic("S001", std::move(message), {start, at_});
    }
    std::pair<char32_t, std::size_t> decode() const {
        if (done()) return {0, 0};
        const auto first = static_cast<unsigned char>(source_[at_.offset]);
        if (first < 0x80) return {first, 1};
        const std::size_t size = first >= 0xc2 && first <= 0xdf ? 2 :
            first >= 0xe0 && first <= 0xef ? 3 : first >= 0xf0 && first <= 0xf4 ? 4 : 0;
        if (!size || at_.offset + size > source_.size()) fail("Invalid UTF-8 input", at_);
        char32_t cp = first & (size == 2 ? 0x1f : size == 3 ? 0x0f : 0x07);
        for (std::size_t i = 1; i < size; ++i) {
            const auto next = static_cast<unsigned char>(source_[at_.offset + i]);
            if ((next & 0xc0) != 0x80) fail("Invalid UTF-8 input", at_);
            cp = (cp << 6) | (next & 0x3f);
        }
        if ((size == 3 && cp < 0x800) || (size == 4 && cp < 0x10000) ||
            (cp >= 0xd800 && cp <= 0xdfff) || cp > 0x10ffff) fail("Invalid UTF-8 input", at_);
        return {cp, size};
    }
    char32_t peek() const { return decode().first; }
    char32_t advance() {
        const auto [cp, size] = decode();
        at_.offset += size;
        if (cp == U'\n') { ++at_.line; at_.column = 1; }
        else if (size) ++at_.column;
        return cp;
    }
    std::string raw(Position start) const { return std::string(source_.substr(start.offset, at_.offset - start.offset)); }
    void emit(TokenKind kind, std::string value, Position start) {
        tokens_.push_back({kind, std::move(value), raw(start), {start, at_}});
    }
    char32_t hex4(Position opening) {
        char32_t result = 0;
        for (int i = 0; i < 4; ++i) {
            const auto cp = peek();
            const int value = digit(cp) ? static_cast<int>(cp - U'0') :
                cp >= U'a' && cp <= U'f' ? static_cast<int>(cp - U'a') + 10 :
                cp >= U'A' && cp <= U'F' ? static_cast<int>(cp - U'A') + 10 : -1;
            if (value < 0) fail("Expected four hexadecimal digits after \\u", opening);
            advance();
            result = result * 16 + static_cast<char32_t>(value);
        }
        return result;
    }
    std::string text(Position opening) {
        advance(); // opening quote
        std::string value;
        while (!done()) {
            auto cp = peek();
            if (cp == U'"') { advance(); return value; }
            if (cp < 0x20) fail("Use an escape for control characters; text must end on the same line", at_);
            advance();
            if (cp != U'\\') { append_utf8(value, cp); continue; }
            if (done()) fail("Unfinished text escape", at_);
            cp = advance();
            switch (cp) {
            case U'"': value += '"'; break;
            case U'\\': value += '\\'; break;
            case U'/': value += '/'; break;
            case U'b': value += '\b'; break;
            case U'f': value += '\f'; break;
            case U'n': value += '\n'; break;
            case U'r': value += '\r'; break;
            case U't': value += '\t'; break;
            case U'u': {
                auto scalar = hex4(opening);
                if (scalar >= 0xd800 && scalar <= 0xdbff) {
                    if (peek() != U'\\') fail("A high surrogate requires a low surrogate escape", opening);
                    advance();
                    if (peek() != U'u') fail("A high surrogate requires a low surrogate escape", opening);
                    advance();
                    const auto low = hex4(opening);
                    if (low < 0xdc00 || low > 0xdfff) fail("Invalid low surrogate", opening);
                    scalar = 0x10000 + ((scalar - 0xd800) << 10) + (low - 0xdc00);
                } else if (scalar >= 0xdc00 && scalar <= 0xdfff) fail("Unexpected low surrogate", opening);
                append_utf8(value, scalar);
                break;
            }
            default: fail("Invalid text escape", at_);
            }
        }
        fail("Unterminated text", opening);
    }
};

std::vector<Token> Lexer::run() {
    while (!done()) {
        const auto start = at_;
        const auto cp = peek();
        if (cp == U' ' || cp == U'\t' || cp == U'\r' || cp == 0xfeff) { advance(); continue; }
        if (cp == U'\n') { advance(); emit(TokenKind::Newline, "\n", start); continue; }
        const auto rest = source_.substr(at_.offset);
        if (cp == U'#') {
            auto directive = rest.substr(0, rest.find('\n'));
            auto tail = directive.size() >= 3 ? directive.substr(3) : std::string_view{};
            while (!tail.empty() && (tail.front() == ' ' || tail.front() == '\t' || tail.front() == '\r')) tail.remove_prefix(1);
            if (start.line != 1 || !tokens_.empty() || (!directive.starts_with("#no") && !directive.starts_with("#en")) || (!tail.empty() && !tail.starts_with("//")))
                fail("Language directive must be #no or #en on the first line", start);
            while (!done() && peek() != U'\n') advance();
            continue;
        }
        if (rest.starts_with("//")) {
            while (!done() && peek() != U'\n') advance();
            continue;
        }
        if (cp == U'"') { auto value = text(start); emit(TokenKind::Text, std::move(value), start); continue; }
        if (digit(cp)) {
            const bool index = !tokens_.empty() && tokens_.back().kind == TokenKind::Symbol && tokens_.back().value == ".";
            do { advance(); } while (digit(peek()));
            auto kind = TokenKind::Integer;
            if (!index && peek() == U'.' && at_.offset + 1 < source_.size() &&
                source_[at_.offset + 1] >= '0' && source_[at_.offset + 1] <= '9') {
                kind = TokenKind::Decimal;
                advance();
                do { advance(); } while (digit(peek()));
            }
            emit(kind, raw(start), start);
            continue;
        }
        if (letter(cp)) {
            do { advance(); } while (continuation(peek()));
            auto spelling = raw(start);
            const auto keyword = canonical_keyword(spelling);
            emit(keyword.empty() ? TokenKind::Identifier : TokenKind::Keyword,
                keyword.empty() ? std::move(spelling) : std::string(keyword), start);
            continue;
        }
        const auto pair = rest.substr(0, 2);
        if (pair == "==") fail("Use 'is' (or 'er') to compare values", start);
        if (pair == "!=") fail("Use 'is not' (or 'er ikke') to compare values", start);
        if (pair == "++" || pair == "--" || pair == "<=" || pair == ">=") {
            advance(); advance(); emit(TokenKind::Symbol, std::string(pair), start); continue;
        }
        if (cp < 128 && "<>:.,()=+-*/%"sv.find(static_cast<char>(cp)) != std::string_view::npos) {
            advance(); emit(TokenKind::Symbol, raw(start), start); continue;
        }
        fail("Unexpected character", start);
    }
    tokens_.push_back({TokenKind::End, "<eof>", "", {at_, at_}});
    return std::move(tokens_);
}
} // namespace

std::string_view canonical_keyword(std::string_view spelling) {
    for (const auto& [alias, canonical] : aliases) if (alias == spelling) return canonical;
    return {};
}
std::vector<Token> tokenize(std::string_view source) { return Lexer(source).run(); }
} // namespace slang
