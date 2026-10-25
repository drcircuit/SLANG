#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>

namespace slang {

// Offsets count UTF-8 bytes; columns count Unicode scalar values, starting at 1.
struct Position {
    std::size_t offset = 0;
    std::size_t line = 1;
    std::size_t column = 1;
};

struct Span {
    Position begin;
    Position end;
};

class Diagnostic final : public std::runtime_error {
public:
    Diagnostic(std::string code, std::string message, Span span)
        : std::runtime_error(std::move(message)), code(std::move(code)), span(span) {}
    std::string code;
    Span span;
    // Runtime closures may fail in source from an earlier console submission.
    std::string source_name;
    std::string source_text;
    bool user_message = false;
};

} // namespace slang
