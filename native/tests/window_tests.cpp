#include "slang/graphics.hpp"
#include "slang/window.hpp"
#include "slang/runtime.hpp"
#include "slang/parser.hpp"
#include <SDL3/SDL.h>
#include <filesystem>
#include <functional>
#include <iostream>
#include <sstream>

namespace {
using namespace slang;
void require(bool value, const std::string& message = "assertion failed") { if (!value) throw std::runtime_error(message); }
SDL_Window* native_window() { int count = 0; auto** windows = SDL_GetWindows(&count); auto* first = count ? windows[0] : nullptr; SDL_free(windows); return first; }
void key(SDL_Scancode code, bool shift = false) {
    SDL_Event e{}; e.type = SDL_EVENT_KEY_DOWN; e.key.windowID = SDL_GetWindowID(native_window()); e.key.scancode = code; e.key.mod = shift ? SDL_KMOD_SHIFT : SDL_KMOD_NONE;
    require(SDL_PushEvent(&e), SDL_GetError());
}
struct Session {
    std::istringstream input; std::ostringstream output; Runtime runtime{input, output};
    void run(const std::string& code) { runtime.execute(parse(code), false, code, "window-test.slang"); }
    void reject(const std::string& code, const std::string& message) {
        try { run(code); } catch (const Diagnostic& e) { require(std::string(e.what()).find(message) != std::string::npos, e.what()); return; }
        throw std::runtime_error("Expected: " + message);
    }
};
}
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const auto root = std::filesystem::absolute(argv[1]); std::filesystem::create_directories(root);
    const std::vector<std::pair<std::string, std::function<void()>>> tests = {
        {"native SDL Unicode text, backspace, Tab and button activation", [] {
            Session s;
            s.run("use window as ui\nui.create<\"Ærlig vindu\">\nvar name = ui.input<\"Navn\">\nfunc Done <>:\nassert<name.value is \"ÆøÅ\">\nprint<name.value>\nui.close<>\n.\nui.button<\"Ferdig\", Done>");
            bool sent = false; RuntimeHost host;
            host.poll = [&] {
                if (sent) return; sent = true;
                SDL_Event e{}; e.type = SDL_EVENT_TEXT_INPUT; e.text.windowID = SDL_GetWindowID(native_window()); e.text.text = "Æøå"; require(SDL_PushEvent(&e));
                key(SDL_SCANCODE_BACKSPACE); e.text.text = "Å"; require(SDL_PushEvent(&e)); key(SDL_SCANCODE_TAB); key(SDL_SCANCODE_RETURN);
            };
            s.runtime.set_host(host); s.run("ui.run<>"); require(s.output.str() == "ÆøÅ\n"); require(!native_window());
        }},
        {"native mouse activation and retained labels", [] {
            Session s; s.run("use window as ui\nui.create<\"Mouse\">\nvar label = ui.label<\"Before\">\nfunc Click <>:\nlabel.caption = \"After\"\nui.close<>\n.\nui.button<\"Click\", Click>");
            SDL_Event e{}; e.type = SDL_EVENT_MOUSE_BUTTON_DOWN; e.button.windowID = SDL_GetWindowID(native_window()); e.button.button = SDL_BUTTON_LEFT; e.button.x = 60; e.button.y = 120;
            require(SDL_PushEvent(&e)); s.run("ui.run<>\nassert<label.caption is \"After\">");
        }},
        {"construction, properties, empty groups and argument errors", [] {
            Session s; s.run("use window as ui"); s.reject("ui.input<\"Name\">", "window.create");
            s.run("ui.create<\"Properties\">\nvar namebox = ui.input<\"Name\", \"Åse\">\nvar choices = ui.checklist<\"Interests\", list<\"Art\", \"Code\">>\nvar empty = ui.combobox<\"Empty\", list<>>\nassert<namebox.value is \"Åse\">\nassert<length<choices.selected> is 0>\nassert<empty.value is none>\nui.close<>");
            s.reject("namebox.value = 3", "Expected text");
            s.run("ui.create<\"New\">"); s.reject("ui.button<\"Bad\", 3>", "needs a function");
            s.run("ui.create<\"New\">"); s.reject("ui.radio_group<\"Bad\", list<\"a\", \"a\">>", "unique");
            s.run("ui.create<\"New\">"); s.reject("ui.listbox<\"Bad\", list<1>>", "text items");
            s.reject("ui.create<\"Small\", 10, 10>", "Window size");
            s.run("ui.create<\"New\">\nvar invalid = ui.input<\"Invalid\">\ninvalid.enabled = none"); s.reject("ui.run<>", "enabled must be bool");
            s.runtime.reset(); require(!native_window());
        }},
        {"SDL close events leave the event loop", [] {
            Session s; s.run("use window\nwindow.create<\"Close\">\nwindow.label<\"Close this window\">");
            SDL_Event e{}; e.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED; e.window.windowID = SDL_GetWindowID(native_window()); require(SDL_PushEvent(&e));
            s.run("window.run<>\nprint<\"closed\">"); require(s.output.str() == "closed\n");
        }},
        {"render all components with the embedded font", [&] {
            Graphics g; Window window(g); window.create("Class club / Klasseklubb", 560, 850);
            auto add = [&](WidgetKind kind, const std::string& text, const std::vector<std::string>& items = {}) -> Widget& {
                Widget widget{}; widget.kind = kind; widget.text = text; widget.items = items; widget.selected.assign(items.size(), false);
                window.widgets().push_back(widget); return window.widgets().back();
            };
            add(WidgetKind::label, "Choose your activities / Velg aktiviteter");
            add(WidgetKind::input, "Your name / Navn").value = "Åse Ørn";
            add(WidgetKind::checklist, "Interests / Interesser", {"Programming / Programmering", "Art / Tegning", "Music / Musikk"}).selected[0] = true;
            auto& radio = add(WidgetKind::radio, "Experience / Erfaring", {"Beginner / Nybegynner", "Explorer / Utforsker"}); radio.choice = 0;
            auto& list = add(WidgetKind::listbox, "Project / Prosjekt", {"Game / Spill", "Weather / Vær", "Robot"}); list.choice = 1;
            auto& combo = add(WidgetKind::combo, "Day / Dag", {"Monday / Mandag", "Tuesday / Tirsdag"}); combo.choice = 0; combo.value = combo.items[0];
            add(WidgetKind::button, "Join the club / Bli med"); window.draw();
            const auto file = (root / "controls.png").u8string(); g.save(std::string(reinterpret_cast<const char*>(file.data()), file.size()));
            require(std::filesystem::file_size(root / "controls.png") > 5000);
        }},
    };
    int failures = 0;
    for (const auto& [name, body] : tests) {
        try { body(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& e) { ++failures; std::cerr << "FAIL " << name << ": " << e.what() << '\n'; }
    }
    return failures ? 1 : 0;
}

