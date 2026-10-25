#include "slang/graphics.hpp"
#include "slang/font.hpp"
#include "slang/library.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstring>
#include <map>
#include <stdexcept>
#include <vector>

namespace slang {
namespace {
void checked(bool success) {
    if (!success) throw std::runtime_error(std::string("Graphics: ") + SDL_GetError());
}
std::string lower(std::string text) {
    for (auto& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}
std::vector<int> codepoints(const std::string& text) {
    const auto offsets = text_offsets(text); std::vector<int> result;
    for (std::size_t i = 0; i + 1 < offsets.size(); ++i) {
        auto begin = offsets[i]; const auto length = offsets[i + 1] - begin;
        int value = static_cast<unsigned char>(text[begin++]) & (length == 1 ? 0x7f : length == 2 ? 0x1f : length == 3 ? 0x0f : 7);
        while (begin < offsets[i + 1]) value = (value << 6) | (static_cast<unsigned char>(text[begin++]) & 63);
        result.push_back(value < 32 ? ' ' : value);
    }
    return result;
}
SDL_Color color(const std::string& text) {
    static const std::map<std::string, SDL_Color> named = {
        {"black", {0,0,0,255}}, {"white", {255,255,255,255}},
        {"red", {255,0,0,255}}, {"green", {0,128,0,255}}, {"blue", {0,0,255,255}},
        {"yellow", {255,255,0,255}}, {"orange", {255,165,0,255}}, {"purple", {128,0,128,255}},
        {"pink", {255,192,203,255}}, {"gray", {128,128,128,255}}, {"grey", {128,128,128,255}},
        {"cyan", {0,255,255,255}}, {"magenta", {255,0,255,255}}
    };
    auto value = text_case(text, false);
    if (auto it = colors_aliases().find(value); it != colors_aliases().end()) value = it->second;
    if (auto it = named.find(value); it != named.end()) return it->second;
    if (value.size() == 7 && value[0] == '#') {
        unsigned rgb = 0;
        for (std::size_t i = 1; i < value.size(); ++i) {
            const char c = value[i];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) throw std::runtime_error("Color must be a name or #RRGGBB");
            rgb = rgb * 16 + static_cast<unsigned>(c <= '9' ? c - '0' : c - 'a' + 10);
        }
        return {static_cast<Uint8>(rgb >> 16), static_cast<Uint8>(rgb >> 8), static_cast<Uint8>(rgb), 255};
    }
    throw std::runtime_error("Unknown color '" + text + "'; use a color name or #RRGGBB");
}
}

struct Graphics::Impl {
    GraphicsHost host;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* pixels = nullptr;
    std::map<std::string, SDL_Texture*> sprites;
    struct CachedGlyph { Glyph glyph; SDL_Texture* texture = nullptr; };
    std::map<int, CachedGlyph> glyphs;
    bool form_mode = false;
    std::vector<WindowEvent> events;
    std::array<bool, SDL_SCANCODE_COUNT> keys{}, snapshot{};
    SDL_Color ink{0, 0, 0, 255};
    bool initialized = false, closed = false, quit = false, frame = false, dirty = false;
    Uint64 last_start = 0, start = 0, elapsed = 16666667, interval = 16666667, next_present = 0;
    std::function<bool()> interrupted;
    explicit Impl(std::function<bool()> stop) : interrupted(std::move(stop)) {}
    ~Impl() { close(); if (initialized) SDL_QuitSubSystem(SDL_INIT_VIDEO); }
    void close() {
        if (form_mode && host.window_mode) host.window_mode(false);
        form_mode = false; events.clear();
        for (auto& [codepoint, glyph] : glyphs) { (void)codepoint; if (glyph.texture) SDL_DestroyTexture(glyph.texture); }
        glyphs.clear();
        for (auto& [path, texture] : sprites) { (void)path; SDL_DestroyTexture(texture); }
        sprites.clear();
        if (pixels) SDL_DestroyTexture(pixels);
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        pixels = nullptr; renderer = nullptr; window = nullptr;
        closed = true; frame = false; dirty = false; keys.fill(false); snapshot.fill(false);
    }
    void create(int width, int height, const std::string& title) {
        if (width < 1 || height < 1 || width > 4096 || height > 4096) throw std::runtime_error("Canvas dimensions must be integers from 1 to 4096");
        if (title.find('\0') != std::string::npos) throw std::runtime_error("Canvas title cannot contain NUL characters");
        close();
        if (!initialized) { checked(SDL_InitSubSystem(SDL_INIT_VIDEO)); initialized = true; }
        try {
            const auto flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | (host.frame ? SDL_WINDOW_HIDDEN : 0);
            checked(SDL_CreateWindowAndRenderer(title.c_str(), width, height, flags, &window, &renderer));
            pixels = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, width, height);
            checked(pixels != nullptr);
            checked(SDL_SetTextureBlendMode(pixels, SDL_BLENDMODE_NONE));
            checked(SDL_SetRenderLogicalPresentation(renderer, width, height, SDL_LOGICAL_PRESENTATION_LETTERBOX));
            checked(SDL_SetRenderTarget(renderer, pixels));
            checked(SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255));
            checked(SDL_RenderClear(renderer));
            closed = false; quit = false; last_start = 0; start = 0; next_present = 0; elapsed = interval;
            dirty = true;
            if (!host.frame) refresh();
        } catch (...) { close(); throw; }
    }
    void ensure() {
        if (window) return;
        if (closed) throw std::runtime_error("Canvas is closed; call graphics.canvas to open another");
        create(640, 480, "SLANG canvas");
    }
    void refresh() {
        if (!window) return;
        if (host.frame) {
            if (host.frame_ready && !host.frame_ready()) return; // One preview frame in flight.
            SDL_Surface* captured = SDL_RenderReadPixels(renderer, nullptr); checked(captured != nullptr);
            SDL_Surface* rgba = SDL_ConvertSurface(captured, SDL_PIXELFORMAT_RGBA32);
            SDL_DestroySurface(captured); checked(rgba != nullptr);
            const int width = rgba->w, height = rgba->h;
            std::vector<std::uint8_t> bytes(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4);
            for (int row = 0; row < height; ++row)
                std::memcpy(bytes.data() + static_cast<std::size_t>(row) * static_cast<std::size_t>(width) * 4,
                    static_cast<std::uint8_t*>(rgba->pixels) + static_cast<std::size_t>(row) * static_cast<std::size_t>(rgba->pitch), static_cast<std::size_t>(width) * 4);
            SDL_DestroySurface(rgba);
            host.frame(width, height, bytes); dirty = false; return;
        }
        checked(SDL_SetRenderTarget(renderer, nullptr));
        checked(SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255));
        checked(SDL_RenderClear(renderer));
        checked(SDL_RenderTexture(renderer, pixels, nullptr, nullptr));
        checked(SDL_RenderPresent(renderer));
        checked(SDL_SetRenderTarget(renderer, pixels));
        dirty = false;
    }
    void service() {
        if (host.poll) host.poll();
        if (!window) return;
        if (host.close_requested && host.close_requested()) { quit = true; close(); return; }
        if (host.window_events) for (auto& event : host.window_events()) if (form_mode && events.size() < 1024) events.push_back(std::move(event));
        if (host.keys) for (const auto& [name, down] : host.keys()) {
            if (name.empty()) { keys.fill(false); continue; }
            const auto key = SDL_GetScancodeFromName(name.c_str());
            if (key > SDL_SCANCODE_UNKNOWN && key < SDL_SCANCODE_COUNT) keys[static_cast<std::size_t>(key)] = down;
        }
        SDL_Event event;
        bool repaint = false;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT || (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))) {
                quit = true; close(); return;
            }
            if ((event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) && event.key.windowID == SDL_GetWindowID(window)) {
                auto key = event.key.scancode;
                if (key > SDL_SCANCODE_UNKNOWN && key < SDL_SCANCODE_COUNT) keys[static_cast<std::size_t>(key)] = event.type == SDL_EVENT_KEY_DOWN;
                if (form_mode && event.type == SDL_EVENT_KEY_DOWN && events.size() < 1024) {
                    const bool control = (event.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0;
                    if (control && key == SDL_SCANCODE_V) {
                        char* pasted = SDL_GetClipboardText();
                        // Ignore oversized pastes instead of cutting a UTF-8 character in half.
                        if (pasted) { if (SDL_strlen(pasted) <= 16384) events.push_back({"text", pasted}); SDL_free(pasted); }
                    } else events.push_back({"key", SDL_GetScancodeName(key), 0, 0, (event.key.mod & SDL_KMOD_SHIFT) != 0, control});
                }
            }
            if (form_mode && events.size() < 1024) {
                if (event.type == SDL_EVENT_TEXT_INPUT && event.text.windowID == SDL_GetWindowID(window)) events.push_back({"text", event.text.text});
                if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.windowID == SDL_GetWindowID(window) && event.button.button == SDL_BUTTON_LEFT) {
                    checked(SDL_ConvertEventToRenderCoordinates(renderer, &event));
                    events.push_back({"click", "", static_cast<int>(event.button.x), static_cast<int>(event.button.y)});
                }
                if (event.type == SDL_EVENT_MOUSE_WHEEL && event.wheel.windowID == SDL_GetWindowID(window))
                    events.push_back({"wheel", "", 0, static_cast<int>(-event.wheel.y * 36)});
            }
            if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST && !host.frame) keys.fill(false);
            if (event.type == SDL_EVENT_WINDOW_EXPOSED || event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) repaint = true;
            if (event.type == SDL_EVENT_RENDER_DEVICE_RESET || event.type == SDL_EVENT_RENDER_TARGETS_RESET)
                throw std::runtime_error("Graphics device was reset; run the program again");
        }
        if (repaint) refresh();
    }
    void stop_check() {
        if (interrupted && interrupted()) throw std::runtime_error("Execution interrupted");
    }
    void drawing() {
        ensure();
        checked(SDL_SetRenderDrawColor(renderer, ink.r, ink.g, ink.b, ink.a));
        dirty = true;
    }
};

Graphics::Graphics(std::function<bool()> interrupted) : impl_(std::make_unique<Impl>(std::move(interrupted))) {}
Graphics::~Graphics() = default;
void Graphics::canvas(int width, int height, const std::string& title) { impl_->create(width, height, title); }
bool Graphics::active() const { return impl_->window != nullptr; }
bool Graphics::user_closed() const { return impl_->quit; }
void Graphics::close() { impl_->close(); }
void Graphics::reset() {
    impl_->close(); impl_->closed = false; impl_->quit = false; impl_->interval = 16666667;
    impl_->elapsed = impl_->interval; impl_->last_start = 0;
    impl_->ink = {0,0,0,255};
}
void Graphics::service() { impl_->service(); }
void Graphics::refresh() { if (impl_->dirty) impl_->refresh(); }
bool Graphics::open() {
    if (impl_->closed) return false;
    impl_->ensure(); impl_->service();
    if (!active()) return false;
    if (impl_->frame) throw std::runtime_error("Finish the current frame with graphics.present before calling graphics.open again");
    impl_->stop_check();
    impl_->start = SDL_GetTicksNS();
    impl_->elapsed = impl_->last_start ? std::min<Uint64>(impl_->start - impl_->last_start, 100000000) : impl_->interval;
    impl_->last_start = impl_->start;
    impl_->snapshot = impl_->keys; impl_->frame = true;
    return true;
}
std::uint64_t Graphics::seconds_ns() const { return impl_->elapsed; }
void Graphics::fps(int rate) {
    if (rate < 1 || rate > 240) throw std::runtime_error("Frame rate must be an integer from 1 to 240");
    impl_->interval = 1000000000ULL / static_cast<Uint64>(rate);
    impl_->next_present = 0;
    if (!impl_->last_start) impl_->elapsed = impl_->interval;
}
void Graphics::present() {
    if (impl_->closed) return;
    impl_->ensure();
    if (!impl_->frame) { if (!open()) return; }
    impl_->refresh();
    const auto deadline = impl_->next_present ? impl_->next_present : impl_->start + impl_->interval;
    do {
        impl_->stop_check(); impl_->service();
        const auto now = SDL_GetTicksNS();
        if (!active() || now >= deadline) break;
        SDL_DelayPrecise(std::min<Uint64>(deadline - now, 2000000));
    } while (true);
    const auto now = SDL_GetTicksNS();
    impl_->next_present = deadline + impl_->interval;
    if (impl_->next_present <= now) impl_->next_present = now + impl_->interval;
    impl_->frame = false;
}
void Graphics::wait() {
    if (impl_->host.frame) {
        // Preserve the final picture even when the previous preview is awaiting
        // acknowledgement. The host retains it after the runner exits.
        while (impl_->dirty && impl_->host.frame_ready && !impl_->host.frame_ready()) { impl_->service(); SDL_Delay(2); }
        refresh(); return;
    }
    refresh();
    while (active()) { impl_->stop_check(); impl_->service(); SDL_Delay(8); }
}
bool Graphics::key_down(const std::string& name) {
    if (name.empty() || name.find('\0') != std::string::npos) throw std::runtime_error("Key name must be non-empty text without NUL characters");
    auto key = SDL_GetScancodeFromName(name.c_str());
    if (key == SDL_SCANCODE_UNKNOWN) throw std::runtime_error("Unknown key '" + name + "'; try right, left, space, escape, or a letter");
    return impl_->snapshot[static_cast<std::size_t>(key)];
}
void Graphics::background(const std::string& name) {
    auto c = color(name); impl_->ensure();
    checked(SDL_SetRenderDrawColor(impl_->renderer, c.r, c.g, c.b, c.a));
    checked(SDL_RenderClear(impl_->renderer)); impl_->dirty = true;
}
void Graphics::fill(const std::string& name) { impl_->ink = color(name); }
void Graphics::line(float x1, float y1, float x2, float y2) {
    impl_->drawing(); checked(SDL_RenderLine(impl_->renderer, x1, y1, x2, y2));
}
void Graphics::rect(float x, float y, float width, float height) {
    if (width < 0 || height < 0) throw std::runtime_error("Rectangle dimensions cannot be negative");
    impl_->drawing(); const SDL_FRect bounds{x, y, width, height}; checked(SDL_RenderFillRect(impl_->renderer, &bounds));
}
void Graphics::circle(float x, float y, float radius) {
    if (radius < 0) throw std::runtime_error("Circle radius cannot be negative");
    impl_->drawing();
    if (radius == 0) return;
    const int segments = static_cast<int>(std::clamp(std::ceil(radius / 2), 16.0f, 128.0f));
    SDL_FColor c{impl_->ink.r / 255.0f, impl_->ink.g / 255.0f, impl_->ink.b / 255.0f, 1};
    std::vector<SDL_Vertex> vertices; std::vector<int> indices;
    vertices.reserve(static_cast<std::size_t>(segments + 2)); indices.reserve(static_cast<std::size_t>(segments * 3));
    vertices.push_back({{x,y}, c, {0,0}});
    for (int i = 0; i <= segments; ++i) {
        const float angle = 6.28318530718f * static_cast<float>(i) / static_cast<float>(segments);
        vertices.push_back({{x + radius * std::cos(angle), y + radius * std::sin(angle)}, c, {0,0}});
        if (i) { indices.push_back(0); indices.push_back(i); indices.push_back(i + 1); }
    }
    checked(SDL_RenderGeometry(impl_->renderer, nullptr, vertices.data(), static_cast<int>(vertices.size()), indices.data(), static_cast<int>(indices.size())));
}
void Graphics::sprite(const std::string& file, float x, float y, float width, float height) {
    impl_->ensure();
    SDL_Texture* texture = nullptr;
    if (auto it = impl_->sprites.find(file); it != impl_->sprites.end()) texture = it->second;
    else {
        const auto path = lower(file);
        SDL_Surface* surface = path.ends_with(".png") ? SDL_LoadPNG(file.c_str()) : path.ends_with(".bmp") ? SDL_LoadBMP(file.c_str()) : nullptr;
        if (!path.ends_with(".png") && !path.ends_with(".bmp")) throw std::runtime_error("Sprites must be PNG or BMP files");
        checked(surface != nullptr);
        texture = SDL_CreateTextureFromSurface(impl_->renderer, surface); SDL_DestroySurface(surface);
        checked(texture != nullptr);
        if (!SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND) || !SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST)) {
            SDL_DestroyTexture(texture); checked(false);
        }
        impl_->sprites.emplace(file, texture);
    }
    float natural_width, natural_height; checked(SDL_GetTextureSize(texture, &natural_width, &natural_height));
    SDL_FRect destination{x,y,width < 0 ? natural_width : width,height < 0 ? natural_height : height};
    checked(SDL_RenderTexture(impl_->renderer, texture, nullptr, &destination)); impl_->dirty = true;
}
void Graphics::save(const std::string& file) {
    if (!lower(file).ends_with(".png")) throw std::runtime_error("Save the canvas to a .png file");
    impl_->ensure();
    SDL_Surface* surface = SDL_RenderReadPixels(impl_->renderer, nullptr); checked(surface != nullptr);
    const bool result = SDL_SavePNG(surface, file.c_str()); SDL_DestroySurface(surface); checked(result);
}
void Graphics::set_host(GraphicsHost host) { impl_->host = std::move(host); }
void Graphics::window_mode(bool enabled) {
    impl_->ensure(); impl_->form_mode = enabled; impl_->events.clear();
    if (!impl_->host.frame) { if (enabled) checked(SDL_StartTextInput(impl_->window)); else checked(SDL_StopTextInput(impl_->window)); }
    if (impl_->host.window_mode) impl_->host.window_mode(enabled);
}
std::vector<WindowEvent> Graphics::window_events() { auto result = std::move(impl_->events); impl_->events.clear(); return result; }
void Graphics::clip(int x, int y, int width, int height) {
    impl_->ensure(); const SDL_Rect rect{x,y,std::max(width, 0),std::max(height, 0)}; checked(SDL_SetRenderClipRect(impl_->renderer, &rect));
}
void Graphics::unclip() { if (impl_->renderer) checked(SDL_SetRenderClipRect(impl_->renderer, nullptr)); }
float Graphics::text_width(const std::string& value) {
    float width = 0; for (int codepoint : codepoints(value)) width += font_advance(codepoint); return width;
}
void Graphics::text(const std::string& value, float x, float y) {
    impl_->drawing();
    for (int codepoint : codepoints(value)) {
        auto found = impl_->glyphs.find(codepoint);
        if (found == impl_->glyphs.end()) {
            if (impl_->glyphs.size() >= 4096) throw std::runtime_error("Window text exceeds 4096 distinct characters");
            Impl::CachedGlyph cached; cached.glyph = font_glyph(codepoint);
            const auto& glyph = cached.glyph;
            if (glyph.width && glyph.height) {
                SDL_Surface* surface = SDL_CreateSurface(glyph.width, glyph.height, SDL_PIXELFORMAT_RGBA32); checked(surface != nullptr);
                for (int row = 0; row < glyph.height; ++row) for (int col = 0; col < glyph.width; ++col) {
                    auto* pixel = static_cast<unsigned char*>(surface->pixels) + row * surface->pitch + col * 4;
                    pixel[0] = pixel[1] = pixel[2] = 255; pixel[3] = glyph.alpha[static_cast<std::size_t>(row * glyph.width + col)];
                }
                cached.texture = SDL_CreateTextureFromSurface(impl_->renderer, surface); SDL_DestroySurface(surface); checked(cached.texture != nullptr);
                checked(SDL_SetTextureBlendMode(cached.texture, SDL_BLENDMODE_BLEND));
            }
            found = impl_->glyphs.emplace(codepoint, std::move(cached)).first;
        }
        auto& cached = found->second; const auto& glyph = cached.glyph;
        if (cached.texture) {
            checked(SDL_SetTextureColorMod(cached.texture, impl_->ink.r, impl_->ink.g, impl_->ink.b));
            const SDL_FRect rect{x + glyph.x, y + 21 + glyph.y, static_cast<float>(glyph.width), static_cast<float>(glyph.height)};
            checked(SDL_RenderTexture(impl_->renderer, cached.texture, nullptr, &rect));
        }
        x += glyph.advance;
    }
}
} // namespace slang
