#pragma once
#include "slang/source.hpp"
#include <string_view>
namespace slang {
std::string source_locale(std::string_view source, std::string_view fallback = "en");
std::string localize_message(std::string_view message, std::string_view locale);
std::string diagnostic_message(const Diagnostic& error, std::string_view source, std::string_view fallback = "en");
}
