#include "slang/ide.hpp"
#include "slang/locale.hpp"
#include "slang/runtime.hpp"
#include "slang/parser.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <thread>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#else
#include <poll.h>
#include <unistd.h>
#endif

namespace slang {
namespace {
void packet(const std::string& json, const std::vector<std::uint8_t>& bytes = {}) {
    const auto size = static_cast<std::uint32_t>(json.size());
    const std::array<char,4> header{static_cast<char>(size), static_cast<char>(size >> 8), static_cast<char>(size >> 16), static_cast<char>(size >> 24)};
    std::cout.write(header.data(), 4); std::cout.write(json.data(), static_cast<std::streamsize>(json.size()));
    if (!bytes.empty()) std::cout.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    std::cout.flush();
}
std::string decode(const std::string& encoded) {
    const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result; unsigned bits = 0; int count = 0;
    for (char c : encoded) {
        if (c == '=') break;
        auto index = alphabet.find(c); if (index == std::string::npos) throw std::runtime_error("Invalid IDE command encoding");
        bits = (bits << 6) | static_cast<unsigned>(index); count += 6;
        if (count >= 8) { count -= 8; result += static_cast<char>((bits >> count) & 255); }
    }
    return result;
}
std::vector<std::string> split(const std::string& text, char delimiter) {
    std::vector<std::string> out; std::size_t start = 0;
    do { auto end = text.find(delimiter, start); out.push_back(text.substr(start, end - start)); if (end == std::string::npos) break; start = end + 1; } while (true);
    return out;
}
std::filesystem::path file_path(const std::string& text) {
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()), text.size()));
}
std::string path_key(std::string text) {
    auto path = file_path(text).lexically_normal().generic_u8string(); text.assign(reinterpret_cast<const char*>(path.data()), path.size());
#ifdef _WIN32
    // Windows file spelling from the picker may differ from an import's spelling.
    for (auto& c : text) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
#endif
    return text;
}
struct Control {
    bool stopped = false, started = false, pause_requested = false, step = false, frame_pending = false, close_canvas = false, input_eof = false;
    bool in_pause = false, awaiting_input = false;
    std::string pending;
    std::deque<std::string> inputs;
    std::vector<std::pair<std::string,bool>> keys;
    std::vector<WindowEvent> window_events;
    std::map<std::string,std::set<std::size_t>> breakpoints;
    DebugLocation last;
    void state(const std::string& name) { packet("{\"type\":\"state\",\"state\":" + json_string(name) + "}"); }
    void command(const std::string& line) {
        auto fields = split(line, '\t'); const auto& name = fields[0];
        if (name == "start") started = true;
        else if (name == "stop") stopped = true;
        else if (name == "pause") pause_requested = true;
        else if (name == "resume") { pause_requested = false; step = false; }
        else if (name == "step") { pause_requested = false; step = true; }
        else if (name == "frame-ack") frame_pending = false;
        else if (name == "canvas-close") close_canvas = true;
        else if (name == "input" && fields.size() == 2) inputs.push_back(decode(fields[1]));
        else if (name == "eof") input_eof = true;
        else if (name == "keysup") { keys.clear(); keys.emplace_back("", false); }
        else if (name == "key" && fields.size() == 3 && fields[1].size() <= 40) {
            if (keys.size() < 512) keys.emplace_back(fields[1], fields[2] == "1");
        } else if (name.starts_with("window-") && window_events.size() < 1024) {
            if (name == "window-text" && fields.size() == 2 && fields[1].size() <= 22000) window_events.push_back({"text", decode(fields[1])});
            if (name == "window-key" && fields.size() == 4 && fields[1].size() <= 40) window_events.push_back({"key", fields[1], 0, 0, fields[2] == "1", fields[3] == "1"});
            if (name == "window-click" && fields.size() == 3) window_events.push_back({"click", "", std::clamp(std::stoi(fields[1]), -4096, 4096), std::clamp(std::stoi(fields[2]), -4096, 4096)});
            if (name == "window-wheel" && fields.size() == 2) window_events.push_back({"wheel", "", 0, std::clamp(std::stoi(fields[1]), -512, 512)});
        } else if (name == "breakpoints" && fields.size() == 3) {
            auto& lines = breakpoints[path_key(decode(fields[1]))]; lines.clear();
            for (const auto& value : split(fields[2], ',')) if (!value.empty()) {
                const auto number = std::stoull(value); if (number && number <= 1000000) lines.insert(static_cast<std::size_t>(number));
            }
        }
    }
    void pump() {
        std::array<char,8192> bytes;
        for (int batch = 0; batch < 32; ++batch) {
            std::size_t received = 0;
#ifdef _WIN32
            const auto handle = GetStdHandle(STD_INPUT_HANDLE); DWORD available = 0, read = 0;
            if (!PeekNamedPipe(handle, nullptr, 0, nullptr, &available, nullptr)) { stopped = true; break; }
            if (!available) break;
            if (!ReadFile(handle, bytes.data(), static_cast<DWORD>(std::min<std::size_t>(available, bytes.size())), &read, nullptr)) { stopped = true; break; }
            received = read;
#else
            pollfd descriptor{STDIN_FILENO, POLLIN, 0};
            if (::poll(&descriptor, 1, 0) <= 0) break;
            const auto count = ::read(STDIN_FILENO, bytes.data(), bytes.size());
            if (count <= 0) { stopped = true; break; }
            received = static_cast<std::size_t>(count);
#endif
            pending.append(bytes.data(), received);
            if (pending.size() > 1048576) throw std::runtime_error("IDE command exceeds size limit");
            std::size_t end;
            while ((end = pending.find('\n')) != std::string::npos) {
                auto line = pending.substr(0, end); pending.erase(0, end + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                command(line);
            }
        }
        if (stopped) throw std::runtime_error("Execution stopped");
    }
    void paused(const DebugInspector& inspect = {}, const std::string& reason = "pause") {
        in_pause = true; pause_requested = true; step = false;
        std::string message = "{\"type\":\"paused\",\"reason\":" + json_string(reason) + ",\"file\":" + json_string(last.file) +
            ",\"line\":" + std::to_string(last.line) + ",\"column\":" + std::to_string(last.column) + ",\"depth\":" + std::to_string(last.depth) + ",\"variables\":[";
        bool first = true;
        if (inspect) for (const auto& variable : inspect()) {
            if (!first) message += ','; first = false;
            message += "{\"name\":" + json_string(variable.name) + ",\"type\":" + json_string(variable.type) + ",\"value\":" + json_string(variable.value) + "}";
        }
        packet(message + "]}");
        while (pause_requested) { pump(); std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
        in_pause = false; state(awaiting_input ? "input" : "running");
    }
    void poll() { pump(); if (pause_requested && !in_pause) paused(); }
    void checkpoint(const DebugLocation& location, const DebugInspector& inspector) {
        pump(); last = location;
        const auto found = breakpoints.find(path_key(last.file));
        const bool breakpoint = found != breakpoints.end() && found->second.contains(last.line);
        if (step || pause_requested || breakpoint) paused(inspector, breakpoint ? "breakpoint" : step ? "step" : "pause");
    }
    std::optional<std::string> input() {
        awaiting_input = true; state("input");
        while (inputs.empty() && !input_eof) { poll(); std::this_thread::sleep_for(std::chrono::milliseconds(4)); }
        awaiting_input = false; state("running");
        if (inputs.empty()) return {};
        auto value = std::move(inputs.front()); inputs.pop_front(); return value;
    }
};
class ConsoleBuffer : public std::streambuf {
    std::string text;
    void flush() { if (!text.empty()) { packet("{\"type\":\"console\",\"text\":" + json_string(text) + "}"); text.clear(); } }
    int_type overflow(int_type c) override {
        if (traits_type::eq_int_type(c, traits_type::eof())) return traits_type::not_eof(c);
        const char value = traits_type::to_char_type(c);
        // Flush before the next UTF-8 character, never in its continuation bytes.
        if (text.size() >= 4096 && (static_cast<unsigned char>(value) & 0xc0) != 0x80) flush();
        text += value;
        if (value == '\n') flush();
        return c;
    }
    int sync() override { flush(); return 0; }
public:
    ~ConsoleBuffer() override { flush(); }
};
}

int run_ide(const std::vector<std::string>& args) {
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    Control control;
    std::string path, snapshot, source, locale = "en";
    for (std::size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--debug") control.step = true;
        else if (args[i] == "--locale" && i + 1 < args.size()) { locale = args[++i]; if (locale != "no" && locale != "en") return 2; }
        else if (args[i] == "--snapshot" && i + 1 < args.size()) snapshot = args[++i];
        else if (path.empty()) path = args[i];
        else { packet("{\"type\":\"error\",\"message\":\"Invalid IDE runner arguments\"}"); return 2; }
    }
    if (path.empty()) { packet("{\"type\":\"error\",\"message\":\"IDE runner requires a source path\"}"); return 2; }
    try {
        packet("{\"type\":\"ready\",\"protocol\":1}");
        while (!control.started) { control.pump(); std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
        std::ifstream file(file_path(snapshot.empty() ? path : snapshot), std::ios::binary);
        if (!file) throw std::runtime_error("Cannot open source file");
        source.assign(std::istreambuf_iterator<char>(file), {});
        ConsoleBuffer buffer; std::ostream console(&buffer); std::istringstream unused_input;
        Runtime runtime(unused_input, console, [&] { return control.stopped; });
        RuntimeHost host;
        host.poll = [&] { control.poll(); };
        host.checkpoint = [&](const auto& location, const auto& inspector) { control.checkpoint(location, inspector); };
        host.input = [&] { return control.input(); };
        host.graphics.poll = host.poll;
        host.graphics.window_events = [&] { auto events = std::move(control.window_events); control.window_events.clear(); return events; };
        host.graphics.window_mode = [&](bool enabled) { packet(std::string("{\"type\":\"window-mode\",\"enabled\":") + (enabled ? "true}" : "false}")); };
        host.graphics.frame_ready = [&] { control.poll(); return !control.frame_pending; };
        host.graphics.frame = [&](int width, int height, const auto& bytes) {
            packet("{\"type\":\"frame\",\"width\":" + std::to_string(width) + ",\"height\":" + std::to_string(height) + ",\"binaryBytes\":" + std::to_string(bytes.size()) + "}", bytes);
            control.frame_pending = true;
        };
        host.graphics.keys = [&] { auto keys = std::move(control.keys); control.keys.clear(); return keys; };
        host.graphics.close_requested = [&] { const bool close = control.close_canvas; control.close_canvas = false; return close; };
        runtime.set_host(std::move(host)); control.state("running");
        runtime.execute(parse(source), false, source, path);
        runtime.wait_for_graphics(); console.flush();
        packet("{\"type\":\"finished\",\"status\":\"success\"}"); return 0;
    } catch (const Diagnostic& error) {
        if (!control.stopped) packet("{\"type\":\"error\",\"file\":" + json_string(error.source_name.empty() ? path : error.source_name) +
            ",\"line\":" + std::to_string(error.span.begin.line) + ",\"column\":" + std::to_string(error.span.begin.column) + ",\"code\":" + json_string(error.code) + ",\"message\":" + json_string(diagnostic_message(error, source, locale)) + "}");
    } catch (const std::exception& error) {
        if (!control.stopped) packet("{\"type\":\"error\",\"message\":" + json_string(localize_message(error.what(), source_locale(source, locale))) + "}");
    }
    packet(std::string("{\"type\":\"finished\",\"status\":\"") + (control.stopped ? "stopped" : "error") + "\"}");
    return control.stopped ? 0 : 1;
}
} // namespace slang
