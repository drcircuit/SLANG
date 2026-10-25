#include "slang/graphics.hpp"
#include "slang/runtime.hpp"
#include "slang/parser.hpp"
#include "slang/library.hpp"
#include <SDL3/SDL.h>
#include <filesystem>
#include <functional>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
using namespace slang;
std::filesystem::path root;
std::string path_text(const std::filesystem::path& path) {
    auto value = path.u8string(); return {reinterpret_cast<const char*>(value.data()), value.size()};
}
void require(bool condition, const std::string& message = "assertion failed") { if (!condition) throw std::runtime_error(message); }
void pixel(const std::string& file, int x, int y, int r, int g, int b, int width = 640, int height = 480) {
    SDL_Surface* image = SDL_LoadPNG(path_text(root / file).c_str()); require(image != nullptr, SDL_GetError());
    Uint8 red, green, blue, alpha;
    const bool correct_size = image->w == width && image->h == height;
    const bool read = SDL_ReadSurfacePixel(image, x, y, &red, &green, &blue, &alpha);
    SDL_DestroySurface(image);
    require(correct_size && read, "Image size/readback failed");
    require(red == r && green == g && blue == b && alpha == 255, "Unexpected pixel in " + file);
}
void make_sprite(const std::string& file, Uint8 red, Uint8 green, Uint8 blue) {
    SDL_Surface* image = SDL_CreateSurface(8, 8, SDL_PIXELFORMAT_RGBA32); require(image != nullptr);
    require(SDL_FillSurfaceRect(image, nullptr, SDL_MapSurfaceRGBA(image, red, green, blue, 255)));
    require(SDL_SavePNG(image, path_text(root / file).c_str())); SDL_DestroySurface(image);
}
SDL_Window* window() {
    int count = 0; SDL_Window** windows = SDL_GetWindows(&count);
    SDL_Window* result = count ? windows[0] : nullptr; SDL_free(windows); return result;
}
struct Session {
    std::istringstream input; std::ostringstream output;
    Runtime runtime;
    explicit Session(std::size_t budget = 1000000) : runtime(input, output, {}, budget) {}
    void run(const std::string& source, const std::string& name = "main.slang") { runtime.execute(parse(source), false, source, path_text(root / name)); }
    void reject(const std::string& source, const std::string& message) {
        try { run(source); }
        catch (const Diagnostic& error) { require(std::string(error.what()).find(message) != std::string::npos, error.what()); return; }
        throw std::runtime_error("Expected rejection: " + message);
    }
};
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    std::cout << std::unitbuf;
    root = std::filesystem::absolute(argv[1]); std::filesystem::create_directories(root);
    const std::vector<std::pair<std::string, std::function<void()>>> tests = {
        {"import is lazy; first shape creates a persistent default canvas", [] {
            Session s; s.run("use graphics as g\ng.fill<\"blue\">"); require(!window());
            s.run("g.circle<100, 100, 25>\ng.save<\"implicit.png\">");
            pixel("implicit.png", 100,100,0,0,255); pixel("implicit.png", 0,0,255,255,255);
            s.run("g.fill<\"red\">\ng.rect<150, 90, 20, 20>\ng.save<\"persistent.png\">");
            pixel("persistent.png",100,100,0,0,255); pixel("persistent.png",155,95,255,0,0);
            s.runtime.reset(); require(!window());
        }},
        {"Norwegian and English colors share RGB values in both drawing operations", [] {
            struct Color { const char* no; const char* en; int r, g, b; };
            const std::vector<Color> colors = {
                {"svart", "black", 0,0,0}, {"hvit", "white", 255,255,255},
                {"rød", "red", 255,0,0}, {"grønn", "green", 0,128,0},
                {"blå", "blue", 0,0,255}, {"gul", "yellow", 255,255,0},
                {"oransje", "orange", 255,165,0}, {"lilla", "purple", 128,0,128},
                {"rosa", "pink", 255,192,203}, {"grå", "grey", 128,128,128},
                {"cyan", "cyan", 0,255,255}, {"magenta", "magenta", 255,0,255}
            };
            Session s; s.run("bruk grafikk som g\ng.lerret<16, 16>");
            for (const auto& c : colors) for (bool upper : {false, true}) for (bool swap : {false, true}) {
                const auto background = text_case(swap ? c.en : c.no, upper);
                const auto fill = text_case(swap ? c.no : c.en, upper);
                s.run("g.bakgrunn<\"" + background + "\">\ng.fyll<\"" + fill + "\">\ng.rektangel<0, 0, 8, 8>\ng.lagre<\"colors.png\">");
                pixel("colors.png", 1,1,c.r,c.g,c.b,16,16);
                pixel("colors.png", 15,15,c.r,c.g,c.b,16,16);
            }
            s.run("g.bakgrunn<\"GRAY\">\ng.fyll<\"#Aa12Ff\">\ng.rektangel<0, 0, 8, 8>\ng.lagre<\"colors.png\">");
            pixel("colors.png", 1,1,0xaa,0x12,0xff,16,16);
            pixel("colors.png", 15,15,128,128,128,16,16);
        }},
        {"present and resize preserve canvas pixels", [] {
            Graphics g; g.canvas(80,60,"retained"); g.fps(240); g.fill("#123456"); g.rect(10,10,20,20); g.present();
            require(SDL_SetWindowSize(window(), 160,120)); g.service();
            g.line(40,5,40,20); g.save(path_text(root / "retained.png"));
            pixel("retained.png",15,15,0x12,0x34,0x56,80,60);
            pixel("retained.png",0,0,255,255,255,80,60);
        }},
        {"ordinary loops reset frame budgets and close cleanly", [] {
            Session s(200); s.run("use graphics as g\ng.fps<240>\nvar frames = 0\nwhile g.open<>:\nassert<g.seconds<> > 0>\nassert<type<g.seconds<>> is \"dec\">\ng.background<\"white\">\ng.circle<20, 20, 5>\nframes++\nif frames is 30:\ng.close<>\n.\ng.present<>\n.\nprint<frames>\nassert<not g.open<>>");
            require(s.output.str() == "30\n"); require(!window());
        }},
        {"callbacks retain closures and survive collection during updates", [] {
            Session s; s.run("use graphics as g\ng.fps<240>\nvar finished = 0\nfunc Make <>:\nvar frames = 0\nret func <seconds as dec>:\nassert<seconds > 0>\nrepeat 1200 times:\nvar scratch = obj<value = list<1, 2>>\n.\nframes++\nfinished = frames\ng.background<\"white\">\ng.circle<frames * 10, 20, 5>\nif frames is 4:\ng.close<>\n.\n.\n.\ng.run<Make<>>\nprint<finished>");
            require(s.output.str() == "4\n"); require(!window());
        }},
        {"callback errors keep their source and release the canvas", [] {
            Session s; const std::string definition = "use graphics as g\nfunc Frame <dt>:\nerror<\"bad frame\">\n.";
            s.run(definition, "definition.slang");
            try { s.run("g.run<Frame>"); throw std::runtime_error("Expected callback failure"); }
            catch (const Diagnostic& error) { require(std::string(error.what()) == "bad frame"); require(error.source_name == path_text(root / "definition.slang")); require(error.span.begin.line == 3); }
            require(!window()); s.run("print<42>"); require(s.output.str() == "42\n");
        }},
        {"invalid callbacks and nested frame control are diagnosed", [] {
            Session s; s.run("use graphics as g");
            s.reject("g.run<42>", "needs a function");
            s.runtime.reset(); s.run("use graphics as g");
            s.reject("g.run<func <dt>:\ng.present<>\n.\n>", "manages frames"); require(!window());
            s.runtime.reset(); s.run("use graphics as g\nfunc Frame <dt>:\ng.run<Frame>\n.");
            s.reject("g.run<Frame>", "Cannot nest");
            s.runtime.reset(); s.run("use graphics as g");
            s.reject("g.run<func <>:\n.\n>", "No overload");
        }},
        {"sprites are cached and share the retained canvas", [] {
            make_sprite("sprite.png",255,0,0);
            Session s; s.run("use graphics as g\ng.sprite<\"sprite.png\", 5, 5>");
            make_sprite("sprite.png",0,0,255);
            s.run("g.sprite<\"sprite.png\", 20, 5, 16, 16>\ng.save<\"sprites.png\">");
            pixel("sprites.png",6,6,255,0,0); pixel("sprites.png",21,6,255,0,0);
        }},
        {"key snapshots and close events", [] {
            Graphics g; g.canvas(80,60,"input"); g.fps(240);
            SDL_Event event{}; event.type = SDL_EVENT_KEY_DOWN; event.key.windowID = SDL_GetWindowID(window()); event.key.scancode = SDL_SCANCODE_RIGHT;
            require(SDL_PushEvent(&event)); require(g.open()); require(g.key_down("right"));
            event.type = SDL_EVENT_KEY_UP; require(SDL_PushEvent(&event)); g.service();
            require(g.key_down("right")); g.present(); require(g.open()); require(!g.key_down("right")); g.present();
            event = {}; event.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED; event.window.windowID = SDL_GetWindowID(window());
            require(SDL_PushEvent(&event)); require(!g.open()); require(!g.active()); require(!g.open());
        }},
        {"invalid graphics values fail with source diagnostics", [] {
            for (const auto& source : {"g.canvas<0, 100>", "g.canvas<1.5, 100>", "g.circle<0, 0, -1>", "g.rect<0, 0, -1, 2>", "g.circle<99999999999999999999999999, 0, 1>", "g.fill<\"invisiblebanana\">", "g.fps<0>", "g.fps<241>", "g.sprite<\"missing.png\", 0, 0>", "g.key_down<\"notakey\">", "g.circle<\"x\", 1, 2>"}) {
                Session s; s.reject(std::string("use graphics as g\n") + source, ""); require(!window());
            }
        }},
        {"stuck graphics updates still hit the execution budget", [] {
            Session s(1000); s.reject("use graphics as g\ng.circle<10, 10, 2>\nwhile true:\n.\n", "Execution limit"); require(!window());
        }},
        {"frame pacing and static window waits can be interrupted", [] {
            bool stopped = false; Graphics g([&] { return stopped; }); g.circle(10,10,2); stopped = true;
            try { g.wait(); throw std::runtime_error("Expected interruption"); }
            catch (const std::runtime_error& error) { require(std::string(error.what()) == "Execution interrupted"); }
        }},
        {"a static result stays alive until a close event", [] {
            Session s; s.run("use graphics as g\ng.circle<10, 10, 2>"); require(window());
            SDL_Event event{}; event.type = SDL_EVENT_QUIT; require(SDL_PushEvent(&event));
            s.runtime.wait_for_graphics(); require(!window());
        }}
    };
    int failures = 0;
    for (const auto& [name, test] : tests) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) { ++failures; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
    }
    return failures ? 1 : 0;
}
