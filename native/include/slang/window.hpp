#pragma once
#include "slang/graphics.hpp"
#include <string>
#include <vector>
namespace slang {
enum class WidgetKind { label, input, button, checklist, radio, listbox, combo };
struct Widget {
    WidgetKind kind;
    std::string text, value;
    std::vector<std::string> items;
    std::vector<bool> selected;
    bool enabled = true;
    int choice = -1, option = 0, first = 0;
    std::size_t cursor = 0;
    bool select_all = false;
    int top = 0, height = 0;
};
// Retained controls with automatic vertical layout. Runtime values are kept by
// the interpreter; this class only owns presentation and interaction state.
class Window {
public:
    explicit Window(Graphics& graphics) : graphics_(graphics) {}
    void create(const std::string& title, int width, int height);
    void close();
    void reset();
    bool active() const { return configured_ && graphics_.active(); }
    bool configured() const { return configured_; }
    std::vector<Widget>& widgets() { return widgets_; }
    std::vector<std::size_t> update();
    void draw();
private:
    Graphics& graphics_;
    std::vector<Widget> widgets_;
    std::string title_;
    int width_ = 480, height_ = 640, focus_ = -1, dropdown_ = -1, scroll_ = 0, total_ = 0;
    bool configured_ = false;
    void layout();
    void focus(int index);
    void reveal();
    void activate(Widget& widget, int option);
    int dropdown_top() const;
    int rows(const Widget& widget) const;
};
} // namespace slang
