#pragma once

#include "slang/ast.hpp"
#include <string_view>

namespace slang {

// Syntax analysis only. Name resolution, type checking, and execution are later stages.
NodePtr parse(std::string_view source);

} // namespace slang
