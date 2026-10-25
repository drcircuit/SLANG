#pragma once
#include "slang/host.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace slang {
// Native canvas owned by one runner, used only on its main thread. No SDL
// handles or platform event types are exposed to the language runtime.
class Graphics {
public:
    explicit Graphics(std::function<bool()> interrupted = {});
    ~Graphics();
    Graphics(const Graphics&) = delete;
    Graphics& operator=(const Graphics&) = delete;
    void canvas(int width, int height, const std::string& title);
    bool open();
    void present();
    void refresh();
    void service();
    void wait();
    void close();
    void reset();
    bool active() const;
    bool user_closed() const;
    std::uint64_t seconds_ns() const;
    void fps(int rate);
    bool key_down(const std::string& key);
    void background(const std::string& color);
    void fill(const std::string& color);
    void line(float x1, float y1, float x2, float y2);
    void rect(float x, float y, float width, float height);
    void circle(float x, float y, float radius);
    void sprite(const std::string& file, float x, float y, float width = -1, float height = -1);
    void save(const std::string& file);
    void set_host(GraphicsHost host);
    void window_mode(bool enabled);
    std::vector<WindowEvent> window_events();
    void text(const std::string& value, float x, float y);
    float text_width(const std::string& value);
    void clip(int x, int y, int width, int height);
    void unclip();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace slang
