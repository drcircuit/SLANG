#include "slang/parser.hpp"
#include "slang/runtime.hpp"
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
using namespace slang;
std::filesystem::path root;
std::string text_path(const std::filesystem::path& path) {
    auto text = path.u8string(); return {reinterpret_cast<const char*>(text.data()), text.size()};
}
void require(bool condition, const std::string& message = "assertion failed") { if (!condition) throw std::runtime_error(message); }
void file(const std::string& name, const std::string& text) {
    auto path = root / std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(name.data()), name.size()));
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary); out << text; require(bool(out), "Cannot write test fixture");
}
struct Session {
    std::istringstream input;
    std::ostringstream output;
    Runtime runtime{input, output};
    void run(const std::string& source) { runtime.execute(parse(source), false, source, text_path(root / "main.slang")); }
    void reject(const std::string& source, const std::string& message) {
        try { run(source); } catch (const Diagnostic& error) { require(std::string(error.what()).find(message) != std::string::npos, error.what()); return; }
        throw std::runtime_error("Expected error: " + message);
    }
};
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    root = std::filesystem::absolute(argv[1]);
    const std::vector<std::pair<std::string, std::function<void()>>> tests = {
        {"private helpers and public functions", [] {
            file("maths.slang", "lib maths:\nfunc Validate <val>:\nassert<val >= 0>\n.\npublic func Square <val>:\nValidate<val>\nret val * val\n.\n.\n");
            Session s; s.run("use \"maths.slang\" as maths\nprint<maths.Square<5>>");
            require(s.output.str() == "25\n");
            s.reject("maths.Validate<1>", "does not export 'Validate'");
            s.reject("Validate<1>", "Unknown name");
            s.reject("maths.1", "named exports");
            s.reject("obj(maths)", "Expected obj");
            s.reject("maths<>", "Only functions");
        }},
        {"library declarations enforce syntax placement", [] {
            Session s;
            s.reject("public func F <>:\n.", "only allowed directly inside lib");
            s.reject("lib m:\npublic var x = 1\n.", "named func or obj");
            s.reject("lib m:\nfunc F <>:\npublic func G <>:\n.\n.\n.", "only allowed directly inside lib");
            s.reject("func F <>:\nuse console\n.", "top level or directly inside lib");
            s.reject("lib a:\nlib b:\n.\n.", "top level");
            s.reject("use \"maths.slang\"", "Expected 'as'");
            s.reject("lib a:\n.\nlib b:\n.", "Only one lib");
        }},
        {"qualified exported types and aliases preserve nominal identity", [] {
            file("players.slang", "lib players:\npublic obj Player:\nname as text\nhealth as int\n.\npublic func Create <name as text>:\nret Player<name, 100>\n.\n.\n");
            Session s;
            s.run("use \"players.slang\" as players\nuse \"./players.slang\" as other\nvar p as players.Player = other.Create<\"Arne\">\nfunc Name <p as other.Player>:\nret p.name\n.\nprint<Name<p>, p.health>\nvar people as list of players.Player = list<p>\nprint<people.1.name>");
            require(s.output.str() == "Arne 100\nArne\n");
            s.reject("var bad as players.Missing = none", "does not export");
            s.reject("var bad2 as players.Create = none", "not an object type");
        }},
        {"public namespace and imports are read-only", [] {
            Session s; s.run("use console\nuse numbers as math");
            s.run("console.print<math.round<2.355, 2>, type<console>>");
            require(s.output.str() == "2.36 lib\n");
            s.reject("console.print = 1", "read-only");
            s.reject("console = obj<>", "Cannot assign");
            s.reject("console.round<1, 0>", "does not export");
            s.reject("use unavailable", "not available");
        }},
        {"one initialization per canonical file, per session", [] {
            file("counter.slang", "lib counter:\nprint<\"loaded\">\nvar count = 0\npublic func Next <>:\nret ++count\n.\n.\n");
            std::filesystem::create_directories(root / "sub");
            Session s; s.run("use \"counter.slang\" as a\nuse \"sub/../counter.slang\" as b\nprint<a.Next<>, b.Next<>>");
            s.run("use \"./counter.slang\" as c\nprint<c.Next<>>");
            require(s.output.str() == "loaded\n1 2\n3\n");
            s.runtime.reset(); s.run("use \"counter.slang\" as a\nprint<a.Next<>>");
            require(s.output.str() == "loaded\n1 2\n3\nloaded\n1\n");
        }},
        {"relative dependency imports and private imported namespaces", [] {
            file("sub/inner.slang", "lib inner:\npublic func Value <>:\nret 42\n.\n.\n");
            file("sub/outer.slang", "lib outer:\nuse \"inner.slang\" as inner\npublic func Value <>:\nret inner.Value<>\n.\n.\n");
            Session s; s.run("use \"sub/outer.slang\" as outer\nprint<outer.Value<>>");
            require(s.output.str() == "42\n");
            s.reject("outer.inner.Value<>", "does not export 'inner'");
        }},
        {"library scopes cannot see callers or leak their state", [] {
            file("isolated.slang", "lib isolated:\npublic func Read <>:\nret secret\n.\n.\n");
            Session s; s.run("var secret = 10");
            s.reject("use \"isolated.slang\" as isolated", "Unknown name 'secret'");
            s.run("lib local:\nvar secret = 5\npublic func Read <>:\nret secret\n.\n.\nprint<local.Read<>, secret>");
            require(s.output.str() == "5 10\n");
            s.reject("local.secret", "does not export 'secret'");
        }},
        {"public overloads work without exposing private overloads", [] {
            Session s;
            s.run("lib maths:\npublic func Add <a, b>:\nret a + b\n.\npublic func Add <a as int, b as text>:\nret text(a) + b\n.\n.\nprint<maths.Add<2, 3>, maths.Add<2, \"3\">>");
            require(s.output.str() == "5 23\n");
            s.reject("lib bad:\npublic func F <x>:\nret x\n.\nfunc F <x as int>:\nret x\n.\n.", "same public/private visibility");
        }},
        {"source diagnostics retain library filenames", [] {
            file("broken.slang", "lib broken:\npublic func Crash <>:\nret 1 / 0\n.\n.\n");
            Session s; s.run("use \"broken.slang\" as broken");
            try { s.run("broken.Crash<>"); throw std::runtime_error("expected error"); }
            catch (const Diagnostic& e) { require(e.source_name == text_path(std::filesystem::canonical(root / "broken.slang"))); require(e.span.begin.line == 3); require(e.source_text.find("ret 1 / 0") != std::string::npos); }
        }},
        {"imported files require one self-contained lib block", [] {
            file("plain.slang", "func F <>:\n.\n");
            file("outside.slang", "var secret = 1\nlib maths:\n.\n");
            file("two.slang", "lib a:\n.\nlib b:\n.\n");
            Session s;
            for (const auto* name : {"plain", "outside", "two"}) s.reject("use \"" + std::string(name) + ".slang\" as m", "exactly one lib");
        }},
        {"cycles are diagnosed and failed initialization is not repeated", [] {
            file("cycle-a.slang", "lib a:\nuse \"cycle-b.slang\" as b\n.\n");
            file("cycle-b.slang", "lib b:\nuse \"cycle-a.slang\" as a\n.\n");
            Session s; s.reject("use \"cycle-a.slang\" as a", "Circular import");
            file("failure.slang", "lib failure:\nprint<\"once\">\nassert<false, \"failed init\">\n.\n");
            s.reject("use \"failure.slang\" as f", "failed init");
            s.reject("use \"failure.slang\" as f", "failed init");
            require(s.output.str() == "once\n");
            s.runtime.reset();
            file("failure.slang", "lib failure:\npublic func Ready <>:\nret true\n.\n.\n");
            s.run("use \"failure.slang\" as f\nprint<f.Ready<>>");
            require(s.output.str() == "once\ntrue\n");
        }},
        {"imported closures survive collection", [] {
            file("closure.slang", "lib closure:\nvar x = 10\npublic func Make <>:\nret func <>:\nret ++x\n.\n.\n.\n");
            Session s; s.run("use \"closure.slang\" as c\nvar next = c.Make<>");
            s.run("print<next<>>"); s.run("print<next<>>"); require(s.output.str() == "11\n12\n");
        }},
        {"missing imports do not occupy their requested alias", [] {
            Session s; s.reject("use \"does-not-exist.slang\" as item", "Cannot open library");
            s.run("use console as item\nitem.print<42>"); require(s.output.str() == "42\n");
        }},
        {"private object types cannot be named externally", [] {
            file("hidden.slang", "lib hidden:\nobj Secret:\nvalue as int\n.\npublic func Make <>:\nret Secret<1>\n.\n.\n");
            Session s; s.run("use \"hidden.slang\" as hidden\nvar value = hidden.Make<>\nprint<value.value>");
            s.reject("var secret as hidden.Secret = none", "does not export 'Secret'");
        }},
        {"transitive import depth is bounded", [] {
            for (int i = 0; i < 34; ++i) file("deep" + std::to_string(i) + ".slang", "lib deep:\nuse \"deep" + std::to_string(i + 1) + ".slang\" as next\n.\n");
            Session s; s.reject("use \"deep0.slang\" as deep", "import depth limit");
        }},
        {"malformed imported syntax points into the imported source", [] {
            file("syntax-error.slang", "lib bad:\npublic func Broken <>:\nret 1\n.\n");
            Session s;
            try { s.run("use \"syntax-error.slang\" as bad"); throw std::runtime_error("expected error"); }
            catch (const Diagnostic& e) { require(e.source_name.ends_with("syntax-error.slang")); require(e.source_text.find("lib bad:") == 0); }
        }},
        {"namespace copies do not bypass visibility", [] {
            Session s; s.run("use console\nvar copy = console\nvar fn = copy.print\nfn<1>");
            require(s.output.str() == "1\n");
            s.reject("copy.round<1, 2>", "does not export");
            s.reject("copy.print = fn", "read-only");
        }},
        {"qualified type annotations in exported fields", [] {
            file("box.slang", "lib box:\nuse \"players.slang\" as p\npublic obj Box:\nplayer as p.Player\n.\n.\n");
            Session s; s.run("use \"box.slang\" as boxes\nuse \"players.slang\" as players\nvar b = boxes.Box<players.Create<\"Ada\">>\nprint<b.player.name>");
            require(s.output.str() == "Ada\n");
        }},
        {"repeated diamond dependencies initialize only once", [] {
            file("base.slang", "lib base:\nprint<\"base\">\npublic func Value <>:\nret 3\n.\n.\n");
            for (const auto* name : {"left", "right"}) file(std::string(name) + ".slang", "lib side:\nuse \"base.slang\" as base\npublic func Value <>:\nret base.Value<>\n.\n.\n");
            Session s; s.run("use \"left.slang\" as left\nuse \"right.slang\" as right\nprint<left.Value<> + right.Value<>>");
            require(s.output.str() == "base\n6\n");
        }}
    };
    int failures = 0;
    for (const auto& [name, test] : tests) {
        try { test(); std::cout << "PASS " << name << std::endl; }
        catch (const std::exception& e) { ++failures; std::cerr << "FAIL " << name << ": " << e.what() << '\n'; }
    }
    return failures ? 1 : 0;
}
