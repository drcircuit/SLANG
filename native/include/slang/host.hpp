#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace slang {
struct DebugLocation { std::string file; std::size_t line = 1, column = 1, depth = 0; };
struct DebugVariable { std::string name, type, value; };
using DebugInspector = std::function<std::vector<DebugVariable>()>;
struct WindowEvent {
    std::string kind, text;
    int x = 0, y = 0;
    bool shift = false, control = false;
};
struct GraphicsHost {
    // Empty callbacks keep the ordinary native-window behavior.
    std::function<bool()> frame_ready;
    std::function<void(int, int, const std::vector<std::uint8_t>&)> frame;
    std::function<void()> poll;
    std::function<std::vector<std::pair<std::string, bool>>()> keys;
    std::function<bool()> close_requested;
    std::function<std::vector<WindowEvent>()> window_events;
    std::function<void(bool)> window_mode;
};
struct RuntimeHost {
    std::function<void(const DebugLocation&, const DebugInspector&)> checkpoint;
    std::function<void()> poll;
    std::function<std::optional<std::string>()> input;
    GraphicsHost graphics;
};
} // namespace slang
