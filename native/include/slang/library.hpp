#pragma once
#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace slang {
using AliasMap = std::map<std::string, std::string>;
const AliasMap& modules_aliases();
const AliasMap& builtins_aliases();
const AliasMap& windowFields_aliases();
const AliasMap& colors_aliases();
const std::map<std::string, AliasMap>& function_aliases();
// The runtime and editor expose these libraries without adding global names.
const std::map<std::string, std::vector<std::string>>& library_exports();
// Byte offsets for validated Unicode scalar values, including an end sentinel.
std::vector<std::size_t> text_offsets(const std::string& text);
std::string text_case(const std::string& text, bool upper);
std::string text_trim(const std::string& text);
inline constexpr std::size_t library_text_limit = 16 * 1024 * 1024;
inline constexpr std::size_t library_list_limit = 1000000;
} // namespace slang
