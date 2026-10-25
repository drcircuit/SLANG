#pragma once

#include "slang/source.hpp"
#include <string_view>
#include <vector>

namespace slang {

enum class TokenKind { Identifier, Keyword, Integer, Decimal, Text, Symbol, Newline, End };

struct Token {
    TokenKind kind;
    std::string value;
    std::string raw;
    Span span;
};

std::vector<Token> tokenize(std::string_view source);
std::string_view canonical_keyword(std::string_view spelling);

} // namespace slang
