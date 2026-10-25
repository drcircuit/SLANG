#include "slang/window.hpp"
#include "slang/library.hpp"
#include <algorithm>
#include <stdexcept>

namespace slang {
namespace {
bool options(WidgetKind kind) { return kind == WidgetKind::checklist || kind == WidgetKind::radio || kind == WidgetKind::listbox; }
void box(Graphics& g, int x, int y, int w, int h, const std::string& fill, const std::string& border) {
    g.fill(border); g.rect(static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h));
    g.fill(fill); g.rect(static_cast<float>(x + 2), static_cast<float>(y + 2), static_cast<float>(w - 4), static_cast<float>(h - 4));
}
std::string one_line(const std::string& value) {
    std::string out;
    for (unsigned char c : value) if (c >= 32 && c != 127) out += static_cast<char>(c);
    (void)text_offsets(out); return out;
}
}
void Window::create(const std::string& title, int width, int height) {
    if (width < 320 || width > 1600 || height < 240 || height > 1600) throw std::runtime_error("Window size must be 320..1600 wide and 240..1600 high");
    reset(); title_ = title; width_ = width; height_ = height;
    graphics_.canvas(width, height, title); graphics_.window_mode(true); configured_ = true;
}
void Window::close() { configured_ = false; graphics_.close(); }
void Window::reset() { widgets_.clear(); configured_ = false; focus_ = dropdown_ = -1; scroll_ = total_ = 0; }
int Window::rows(const Widget& widget) const { return std::max(1, std::min(4, static_cast<int>(widget.items.size()))); }
void Window::layout() {
    int y = 64;
    for (auto& widget : widgets_) {
        widget.top = y;
        widget.height = widget.kind == WidgetKind::label ? 36 : widget.kind == WidgetKind::button ? 44 : options(widget.kind) ? 32 + rows(widget) * 32 + 8 : 76;
        y += widget.height + 10;
        widget.option = std::clamp(widget.option, 0, std::max(0, static_cast<int>(widget.items.size()) - 1));
        widget.first = std::clamp(widget.first, 0, std::max(0, static_cast<int>(widget.items.size()) - 4));
        widget.cursor = std::min(widget.cursor, widget.value.size());
        const auto offsets = text_offsets(widget.value);
        widget.cursor = *std::lower_bound(offsets.begin(), offsets.end(), widget.cursor);
    }
    total_ = y;
    scroll_ = std::clamp(scroll_, 0, std::max(0, total_ - height_ + 32));
    if (focus_ >= static_cast<int>(widgets_.size()) || (focus_ >= 0 && !widgets_[static_cast<std::size_t>(focus_)].enabled)) focus_ = -1;
    if (focus_ < 0) for (std::size_t i = 0; i < widgets_.size(); ++i) if (widgets_[i].kind != WidgetKind::label && widgets_[i].enabled) { focus(static_cast<int>(i)); break; }
}
void Window::focus(int index) {
    if (focus_ >= 0) widgets_[static_cast<std::size_t>(focus_)].select_all = false;
    focus_ = index; dropdown_ = -1;
    if (focus_ >= 0) {
        auto& widget = widgets_[static_cast<std::size_t>(focus_)]; widget.cursor = widget.value.size();
        if (widget.choice >= 0) widget.option = widget.choice;
    }
}
void Window::reveal() {
    if (focus_ < 0) return;
    const auto& widget = widgets_[static_cast<std::size_t>(focus_)];
    if (widget.top - scroll_ < 64) scroll_ = widget.top - 64;
    if (widget.top + widget.height - scroll_ > height_ - 32) scroll_ = widget.top + widget.height - height_ + 32;
}
void Window::activate(Widget& widget, int option) {
    if (option < 0 || option >= static_cast<int>(widget.items.size())) return;
    widget.option = option;
    if (widget.kind == WidgetKind::checklist) widget.selected[static_cast<std::size_t>(option)] = !widget.selected[static_cast<std::size_t>(option)];
    else { widget.choice = option; widget.value = widget.items[static_cast<std::size_t>(option)]; }
}
int Window::dropdown_top() const {
    const auto& widget = widgets_[static_cast<std::size_t>(dropdown_)];
    const int below = widget.top - scroll_ + 68;
    return below + rows(widget) * 32 <= height_ - 28 ? below : std::max(64, widget.top - scroll_ + 28 - rows(widget) * 32);
}
std::vector<std::size_t> Window::update() {
    layout(); std::vector<std::size_t> clicks;
    for (const auto& event : graphics_.window_events()) {
        if (event.kind == "wheel") {
            if (dropdown_ >= 0) {
                auto& widget = widgets_[static_cast<std::size_t>(dropdown_)];
                widget.first = std::clamp(widget.first + (event.y > 0 ? 1 : -1), 0, std::max(0, static_cast<int>(widget.items.size()) - 4));
            } else scroll_ = std::clamp(scroll_ + event.y, 0, std::max(0, total_ - height_ + 32));
            continue;
        }
        if (event.kind == "click") {
            if (dropdown_ >= 0) {
                auto& widget = widgets_[static_cast<std::size_t>(dropdown_)]; const int y = dropdown_top();
                if (event.x >= 24 && event.x < width_ - 24 && event.y >= y && event.y < y + rows(widget) * 32) {
                    activate(widget, widget.first + (event.y - y) / 32); dropdown_ = -1; continue;
                }
                dropdown_ = -1;
            }
            if (event.x < 24 || event.x >= width_ - 24 || event.y < 60 || event.y >= height_ - 28) continue;
            for (std::size_t i = 0; i < widgets_.size(); ++i) {
                auto& widget = widgets_[i]; const int top = widget.top - scroll_;
                if (!widget.enabled || widget.kind == WidgetKind::label || event.y < top || event.y >= top + widget.height) continue;
                focus(static_cast<int>(i));
                if (widget.kind == WidgetKind::button) clicks.push_back(i);
                else if (options(widget.kind) && event.y >= top + 32 && event.y < top + 32 + rows(widget) * 32) activate(widget, widget.first + (event.y - top - 32) / 32);
                else if (widget.kind == WidgetKind::combo && event.y >= top + 28) dropdown_ = static_cast<int>(i);
                break;
            }
            continue;
        }
        if (event.kind == "key" && event.text == "Tab") {
            const int count = static_cast<int>(widgets_.size()); int next = focus_;
            for (int tries = 0; tries < count; ++tries) {
                next = (next + (event.shift ? count - 1 : 1)) % count;
                if (widgets_[static_cast<std::size_t>(next)].enabled && widgets_[static_cast<std::size_t>(next)].kind != WidgetKind::label) { focus(next); reveal(); break; }
            }
            continue;
        }
        if (event.kind == "key" && event.text == "Escape") { if (dropdown_ >= 0) dropdown_ = -1; else close(); continue; }
        if (focus_ < 0 || !active()) continue;
        auto& widget = widgets_[static_cast<std::size_t>(focus_)];
        if (widget.kind == WidgetKind::input) {
            if (event.kind == "text") {
                const auto text = one_line(event.text);
                const auto size = widget.select_all ? 0 : widget.value.size();
                if (size + text.size() > 4096) continue;
                if (widget.select_all) { widget.value.clear(); widget.cursor = 0; widget.select_all = false; }
                widget.value.insert(widget.cursor, text); widget.cursor += text.size();
            } else if (event.kind == "key") {
                const auto offsets = text_offsets(widget.value);
                const auto at = static_cast<std::size_t>(std::lower_bound(offsets.begin(), offsets.end(), widget.cursor) - offsets.begin());
                if (event.control && event.text == "A") widget.select_all = true;
                else if (event.text == "Backspace" || event.text == "Delete") {
                    if (widget.select_all) { widget.value.clear(); widget.cursor = 0; }
                    else if (event.text == "Backspace" && at) { widget.value.erase(offsets[at - 1], widget.cursor - offsets[at - 1]); widget.cursor = offsets[at - 1]; }
                    else if (event.text == "Delete" && at + 1 < offsets.size()) widget.value.erase(widget.cursor, offsets[at + 1] - widget.cursor);
                    widget.select_all = false;
                } else if (event.text == "Left" || event.text == "Right" || event.text == "Home" || event.text == "End") {
                    if (event.text == "Home") widget.cursor = 0;
                    if (event.text == "End") widget.cursor = widget.value.size();
                    if (event.text == "Left" && at) widget.cursor = offsets[at - 1];
                    if (event.text == "Right" && at + 1 < offsets.size()) widget.cursor = offsets[at + 1];
                    widget.select_all = false;
                }
            }
        } else if (event.kind == "key") {
            const auto& key = event.text;
            if (widget.kind == WidgetKind::button && (key == "Return" || key == "Space")) clicks.push_back(static_cast<std::size_t>(focus_));
            if (options(widget.kind) || widget.kind == WidgetKind::combo) {
                if (key == "Up" || key == "Down" || key == "Home" || key == "End") {
                    int next = key == "Home" ? 0 : key == "End" ? static_cast<int>(widget.items.size()) - 1 : widget.option + (key == "Down" ? 1 : -1);
                    next = std::clamp(next, 0, std::max(0, static_cast<int>(widget.items.size()) - 1));
                    widget.option = next;
                    if (widget.kind != WidgetKind::checklist) activate(widget, next);
                    widget.first = std::clamp(widget.first, std::max(0, next - 3), next);
                }
                if (key == "Space" || key == "Return") {
                    if (widget.kind == WidgetKind::combo) dropdown_ = dropdown_ < 0 ? focus_ : -1;
                    else activate(widget, widget.option);
                }
            }
        }
    }
    return clicks;
}
void Window::draw() {
    if (!active()) return;
    layout(); graphics_.unclip(); graphics_.background("#F1F5F9");
    graphics_.fill("#0F172A"); graphics_.clip(24, 8, width_ - 48, 44); graphics_.text(title_, 24, 16);
    for (std::size_t i = 0; i < widgets_.size(); ++i) {
        auto& widget = widgets_[i]; const int y = widget.top - scroll_, w = width_ - 48;
        if (y + widget.height < 60 || y > height_ - 28) continue;
        graphics_.clip(24, 60, w, height_ - 88);
        const bool focused = static_cast<int>(i) == focus_;
        const auto ink = widget.enabled ? "#172033" : "#64748B";
        const auto border = focused && widget.enabled ? "#2563EB" : "#94A3B8";
        if (widget.kind == WidgetKind::button) {
            box(graphics_, 24, y, w, 40, widget.enabled ? "#1D4ED8" : "#64748B", focused ? "#0F172A" : "#1D4ED8");
            graphics_.fill("white"); graphics_.text(widget.text, 36, static_cast<float>(y + 7));
            continue;
        }
        graphics_.fill(ink); graphics_.text(widget.text, 24, static_cast<float>(y));
        if (widget.kind == WidgetKind::label) continue;
        const int body = y + 28;
        if (widget.kind == WidgetKind::input || widget.kind == WidgetKind::combo) {
            box(graphics_, 24, body, w, 38, "white", border);
            const int top = std::max(60, body + 3), bottom = std::min(height_ - 28, body + 35);
            graphics_.clip(32, top, w - 48, std::max(0, bottom - top));
            float offset = widget.kind == WidgetKind::input && focused ? std::max(0.0f, graphics_.text_width(widget.value.substr(0, widget.cursor)) - (w - 56)) : 0;
            if (widget.select_all && focused) { graphics_.fill("#BFDBFE"); graphics_.rect(32, static_cast<float>(body + 4), graphics_.text_width(widget.value), 28); }
            graphics_.fill(ink); graphics_.text(widget.value, 32 - offset, static_cast<float>(body + 6));
            if (widget.kind == WidgetKind::input && focused) {
                const float caret = 32 + graphics_.text_width(widget.value.substr(0, widget.cursor)) - offset;
                graphics_.fill("#1D4ED8"); graphics_.line(caret, static_cast<float>(body + 7), caret, static_cast<float>(body + 29));
            }
            graphics_.clip(24, 60, w, height_ - 88);
            if (widget.kind == WidgetKind::combo) { graphics_.fill(ink); graphics_.text("v", static_cast<float>(width_ - 48), static_cast<float>(body + 6)); }
        } else {
            box(graphics_, 24, body, w, rows(widget) * 32 + 8, "white", border);
            for (int row = 0; row < rows(widget); ++row) {
                const auto index = static_cast<std::size_t>(widget.first + row); if (index >= widget.items.size()) break;
                const int row_y = body + 4 + row * 32;
                if (static_cast<int>(index) == widget.choice || (focused && static_cast<int>(index) == widget.option)) { graphics_.fill("#DBEAFE"); graphics_.rect(28, static_cast<float>(row_y), static_cast<float>(w - 8), 32); }
                const bool check = widget.kind == WidgetKind::checklist || widget.kind == WidgetKind::radio;
                if (check) {
                    const bool selected = widget.kind == WidgetKind::checklist ? widget.selected[index] : static_cast<int>(index) == widget.choice;
                    if (widget.kind == WidgetKind::radio) {
                        graphics_.fill("#64748B"); graphics_.circle(43, static_cast<float>(row_y + 16), 9);
                        graphics_.fill("white"); graphics_.circle(43, static_cast<float>(row_y + 16), 7);
                        if (selected) { graphics_.fill("#1D4ED8"); graphics_.circle(43, static_cast<float>(row_y + 16), 4); }
                    } else {
                        box(graphics_, 34, row_y + 7, 18, 18, "white", "#64748B");
                        if (selected) { graphics_.fill("#1D4ED8"); graphics_.line(37, static_cast<float>(row_y + 16), 41, static_cast<float>(row_y + 20)); graphics_.line(41, static_cast<float>(row_y + 20), 49, static_cast<float>(row_y + 11)); }
                    }
                }
                graphics_.fill(ink); graphics_.text(widget.items[index], check ? 62.0f : 34.0f, static_cast<float>(row_y + 3));
            }
            if (widget.items.size() > 4) { graphics_.fill("#64748B"); graphics_.rect(static_cast<float>(width_ - 29), static_cast<float>(body + 4 + widget.first * (rows(widget) * 32 - 20) / std::max(1, static_cast<int>(widget.items.size()) - 4)), 3, 20); }
        }
    }
    graphics_.unclip();
    if (dropdown_ >= 0) {
        const auto& widget = widgets_[static_cast<std::size_t>(dropdown_)]; const int y = dropdown_top();
        box(graphics_, 24, y, width_ - 48, rows(widget) * 32, "white", "#2563EB");
        graphics_.clip(28, y + 2, width_ - 56, rows(widget) * 32 - 4);
        for (int row = 0; row < rows(widget); ++row) {
            const auto index = static_cast<std::size_t>(widget.first + row); if (index >= widget.items.size()) break;
            if (static_cast<int>(index) == widget.choice) { graphics_.fill("#DBEAFE"); graphics_.rect(28, static_cast<float>(y + row * 32), static_cast<float>(width_ - 56), 32); }
            graphics_.fill("#172033"); graphics_.text(widget.items[index], 34, static_cast<float>(y + row * 32 + 3));
        }
    }
    graphics_.unclip();
    if (total_ > height_ - 32) {
        graphics_.fill("#CBD5E1"); graphics_.rect(static_cast<float>(width_ - 12), 64, 5, static_cast<float>(height_ - 96));
        graphics_.fill("#64748B"); graphics_.rect(static_cast<float>(width_ - 12), 64.0f + (height_ - 128) * static_cast<float>(scroll_) / std::max(1, total_ - height_ + 32), 5, 32);
    }
    graphics_.fill("#F1F5F9"); graphics_.rect(0, static_cast<float>(height_ - 27), static_cast<float>(width_), 27);
    graphics_.fill("#475569"); graphics_.text("Tab: next control   |   Esc: close", 24, static_cast<float>(height_ - 27));
}
} // namespace slang
