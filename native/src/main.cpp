#include "slang/parser.hpp"
#include "slang/lexer.hpp"
#include "slang/runtime.hpp"
#include "slang/ide.hpp"
#include "slang/locale.hpp"
#include <csignal>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string_view>
#include <vector>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace {
constexpr std::string_view usage =
    "SLANG " SLANG_VERSION "\n"
    "Usage: slang check [--json] [--locale no|en] <files...>\n"
    "       slang ast [--locale no|en] <file>\n"
    "       slang run [--max-steps N] [--locale no|en] <file>\n"
    "       slang repl [--max-steps N]\n"
    "       slang --version\n"
    "Use '-' as the file to read source from standard input.\n"
    "Run executes with runtime type checks. Check/ast are syntax tools.\n"
    "A first-line #no or #en directive overrides --locale for file diagnostics.\n"
    "The REPL accepts English and Norwegian names together.\n"
    "Execution defaults to 1000000 steps per submission; 0 means unlimited.\n";

volatile std::sig_atomic_t interrupted = 0;
void stop_execution(int) { interrupted = 1; }

std::filesystem::path utf8_path(const std::string& path) {
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(path.data()), path.size()));
}
std::string read_source(const std::string& path) {
    if (path == "-") return {std::istreambuf_iterator<char>(std::cin), {}};
    std::ifstream file(utf8_path(path), std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open source file");
    std::string source{std::istreambuf_iterator<char>(file), {}};
    if (file.bad()) throw std::runtime_error("Cannot read source file");
    return source;
}
std::string position_json(const slang::Position& position) {
    return "{\"offset\":" + std::to_string(position.offset) + ",\"line\":" + std::to_string(position.line) +
        ",\"column\":" + std::to_string(position.column) + "}";
}
void json_result(const std::string& file, bool success, std::string_view message = {},
    std::string_view code = {}, const slang::Span* span = nullptr) {
    std::cout << "{\"file\":" << slang::json_string(file) << ",\"success\":" << (success ? "true" : "false")
        << ",\"phase\":\"syntax\",\"diagnostics\":[";
    if (!success) {
        std::cout << "{\"code\":" << slang::json_string(code) << ",\"message\":" << slang::json_string(message);
        if (span) std::cout << ",\"start\":" << position_json(span->begin) << ",\"end\":" << position_json(span->end);
        std::cout << "}";
    }
    std::cout << "]}\n";
}
void show_diagnostic(const std::string& file, std::string_view source, const slang::Diagnostic& error, std::string_view fallback = "en") {
    if (!error.source_name.empty()) source = error.source_text;
    const auto position = error.span.begin;
    const auto locale = slang::source_locale(source, fallback);
    std::cerr << (error.source_name.empty() ? file : error.source_name) << ':' << position.line << ':' << position.column << (locale == "no" ? ": feil [" : ": error [") << error.code << "]: " << slang::diagnostic_message(error, source, fallback) << '\n';
    if (position.offset > source.size()) return;
    const auto before = source.substr(0, position.offset).find_last_of('\n');
    const auto start = before == std::string_view::npos ? 0 : before + 1;
    auto end = source.find('\n', start);
    if (end == std::string_view::npos) end = source.size();
    std::string displayed;
    std::size_t display_column = 0;
    std::size_t caret = 0;
    for (std::size_t i = start; i < end; ++i) {
        if (i == position.offset) caret = display_column;
        const auto c = static_cast<unsigned char>(source[i]);
        if (c == '\r') continue;
        if (c == '\t') {
            const auto width = 4 - display_column % 4;
            displayed.append(width, ' ');
            display_column += width;
        } else {
            displayed += static_cast<char>(c);
            if ((c & 0xc0) != 0x80) ++display_column;
        }
    }
    if (position.offset >= end) caret = display_column;
    std::cerr << "  " << displayed << "\n  " << std::string(caret, ' ') << "^\n";
}

bool incomplete(std::string_view source) {
    try {
        const auto tokens = slang::tokenize(source);
        int blocks = 0, parens = 0, angles = 0;
        bool line_start = true, branch = false;
        for (std::size_t i = 0; i < tokens.size(); ++i) {
            const auto& t = tokens[i];
            if (t.kind == slang::TokenKind::Newline) { line_start = true; branch = false; continue; }
            if (t.kind == slang::TokenKind::End) break;
            if (line_start) branch = t.kind == slang::TokenKind::Keyword && (t.value == "else" || t.value == "else_or");
            if (t.kind == slang::TokenKind::Symbol) {
                if (t.value == ":" && !branch) ++blocks;
                if (t.value == "." && line_start) --blocks;
                if (t.value == "(") ++parens;
                if (t.value == ")") --parens;
                if (t.value == "<") ++angles;
                if (t.value == ">") --angles;
            }
            line_start = false;
        }
        return blocks > 0 || parens > 0 || angles > 0;
    } catch (const slang::Diagnostic&) { return false; }
}

int repl(std::size_t max_steps) {
    slang::Runtime runtime(std::cin, std::cout, [] { return interrupted != 0; }, max_steps);
    std::cout << "SLANG " SLANG_VERSION " interactive console\n:help for help; :quit to leave.\n";
    std::string source, line;
    std::size_t submission = 0;
    while (true) {
        std::cout << (source.empty() ? ">>> " : "... ") << std::flush;
        if (!runtime.read_line(line)) break;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line == ":quit") return 0;
        if (line == ":cancel") { source.clear(); continue; }
        if (line == ":reset") { runtime.reset(); source.clear(); std::cout << "Session reset.\n"; continue; }
        if (line == ":help") {
            std::cout << "Enter expressions or statements; finish blocks with a period on its own line.\n"
                "print<value> displays a value. type<value> shows its type.\n"
                ":cancel discards pending input, :reset clears the session, :quit exits.\n"
                "Blank lines are allowed inside blocks. :submit checks unfinished input. Ctrl+C interrupts execution.\n";
            continue;
        }
        const bool force_submit = line == ":submit";
        if (force_submit && source.empty()) continue;
        if (source.empty() && line.empty()) continue;
        if (!force_submit) source += line + '\n';
        slang::NodePtr program;
        try { program = slang::parse(source); }
        catch (const slang::Diagnostic& error) {
            if (!force_submit && incomplete(source)) continue;
            show_diagnostic("<console>", source, error); source.clear(); continue;
        }
        interrupted = 0;
        try { runtime.execute(program, true, source, "<console:" + std::to_string(++submission) + ">"); }
        catch (const slang::Diagnostic& error) { show_diagnostic("<console>", source, error); }
        catch (const std::exception& error) { std::cerr << "error: " << error.what() << '\n'; }
        source.clear();
    }
    if (!source.empty()) { std::cerr << "Incomplete input at end of console session.\n"; return 1; }
    return 0;
}

int run(const std::vector<std::string>& args) {
    std::signal(SIGINT, stop_execution);
    if (args.empty()) return repl(1000000);
    if (args.size() == 1 && (args[0] == "--help" || args[0] == "-h")) {
        std::cout << usage;
        return 0;
    }
    if (args.size() == 1 && args[0] == "--version") {
        std::cout << "slang " SLANG_VERSION " (native interpreter)\n";
        return 0;
    }
    const auto& command = args[0];
    if (command == "ide") return slang::run_ide(args);
    if (command != "check" && command != "ast" && command != "run" && command != "repl") { std::cerr << usage; return 2; }
    std::size_t max_steps = 1000000;
    bool json = false;
    std::string locale = "en";
    bool options = true;
    std::vector<std::string> paths;
    for (std::size_t i = 1; i < args.size(); ++i) {
        if (options && args[i] == "--") { options = false; continue; }
        if (options && args[i] == "--json" && command == "check") { json = true; continue; }
        if (options && args[i] == "--locale" && command != "repl" && i + 1 < args.size()) {
            locale = args[++i];
            if (locale != "no" && locale != "en") { std::cerr << "--locale must be no or en\n"; return 2; }
            continue;
        }
        if (options && args[i] == "--max-steps" && (command == "run" || command == "repl")) {
            if (++i >= args.size()) { std::cerr << usage; return 2; }
            const auto& value = args[i];
            auto result = std::from_chars(value.data(), value.data() + value.size(), max_steps);
            if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) { std::cerr << "--max-steps needs a non-negative integer\n"; return 2; }
            continue;
        }
        if (options && args[i].size() > 1 && args[i][0] == '-') { std::cerr << usage; return 2; }
        paths.push_back(args[i]);
    }
    if (command == "repl") { if (!paths.empty()) { std::cerr << usage; return 2; } return repl(max_steps); }
    if (paths.empty() || ((command == "ast" || command == "run") && paths.size() != 1)) { std::cerr << usage; return 2; }
    std::size_t stdin_count = 0;
    for (const auto& path : paths) if (path == "-") ++stdin_count;
    if (stdin_count > 1) { std::cerr << "Standard input may only be read once.\n"; return 2; }

    int status = 0;
    for (const auto& path : paths) {
        std::string source;
        try {
            source = read_source(path);
            const auto ast = slang::parse(source);
            if (command == "ast") std::cout << slang::ast_json(ast) << '\n';
            else if (command == "run") {
                slang::Runtime runtime(std::cin, std::cout, [] { return interrupted != 0; }, max_steps);
                runtime.execute(ast, false, source, path);
                runtime.wait_for_graphics();
            }
            else if (json) json_result(path, true);
            else std::cout << path << (slang::source_locale(source, locale) == "no" ? ": syntaks OK (ikke typekontrollert eller kjørt)\n" : ": syntax OK (not type-checked or executed)\n");
        } catch (const slang::Diagnostic& error) {
            status = 1;
            if (json) json_result(path, false, slang::diagnostic_message(error, source, locale), error.code, &error.span);
            else show_diagnostic(path, source, error, locale);
        } catch (const std::exception& error) {
            status = 1;
            const auto selected = slang::source_locale(source, locale);
            if (json) json_result(path, false, slang::localize_message(error.what(), selected), "IO001");
            else std::cerr << path << (selected == "no" ? ": feil: " : ": error: ") << slang::localize_message(error.what(), selected) << '\n';
        }
    }
    return status;
}
} // namespace

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        const auto length = static_cast<int>(std::char_traits<wchar_t>::length(argv[i]));
        const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i], length, nullptr, 0, nullptr, nullptr);
        if (size == 0 && length != 0) { std::cerr << "Invalid Unicode argument\n"; return 2; }
        std::string value(static_cast<std::size_t>(size), '\0');
        if (size) WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, argv[i], length, value.data(), size, nullptr, nullptr);
        args.push_back(std::move(value));
    }
    return run(args);
}
#else
int main(int argc, char** argv) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) args.emplace_back(argv[i]);
    return run(args);
}
#endif
