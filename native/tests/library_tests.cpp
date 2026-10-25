#include "slang/parser.hpp"
#include "slang/runtime.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
using namespace slang;
std::filesystem::path root;
std::string path_text(const std::filesystem::path& path) {
    auto value = path.u8string(); return {reinterpret_cast<const char*>(value.data()), value.size()};
}
void require(bool condition, const std::string& message = "assertion failed") { if (!condition) throw std::runtime_error(message); }
void file(const std::filesystem::path& path, const std::string& value) {
    std::filesystem::create_directories(path.parent_path()); std::ofstream out(path, std::ios::binary); out << value;
    require(bool(out), "Cannot write fixture");
}
struct Session {
    std::istringstream input;
    std::ostringstream output;
    Runtime runtime{input, output};
    void run(const std::string& code) { runtime.execute(parse(code), false, code, path_text(root / "main.slang")); }
    void reject(const std::string& code, const std::string& message) {
        try { run(code); } catch (const Diagnostic& error) {
            require(std::string(error.what()).find(message) != std::string::npos, error.what()); return;
        }
        throw std::runtime_error("Expected error: " + message);
    }
};
}
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    root = std::filesystem::absolute(argv[1]); std::filesystem::create_directories(root);
    const std::vector<std::pair<std::string, std::function<void()>>> tests = {
        {"exact arithmetic and numeric result types", [] {
            Session s; s.run(R"(
use numbers as n
assert<n.abs<-123456789012345678901234567890> is 123456789012345678901234567890>
assert<n.floor<-1.01> is -2>
assert<n.ceil<-1.01> is -1>
assert<n.floor<0.9> is 0>
assert<n.ceil<0.9> is 1>
assert<type<n.floor<3.0>> is "int">
assert<type<n.abs<-3.0>> is "dec">
assert<n.min<3, 1.5> is 1.5>
assert<n.max<3, 1.5> is 3>
assert<type<n.max<3, 1.5>> is "dec">
assert<n.clamp<-1, 0, 10> is 0>
assert<n.clamp<12, 0, 10> is 10>
assert<n.clamp<5, 0, 10> is 5>
assert<n.pow<2, 100> is 1267650600228229401496703205376>
assert<n.pow<0.1, 3> is 0.001>
assert<n.pow<0, 0> is 1>
assert<n.sum<list<0.1, 0.2, 0.3>> is 0.6>
assert<n.sum<list<>> is 0>
assert<n.mean<list<1, 2, 3>> is 2>
assert<type<n.mean<list<1, 2, 3>>> is "dec">
assert<n.sqrt<2> is 1.4142135623730950488016887242>
assert<n.sqrt<2.25, 0> is 2>
assert<n.sqrt<6.25, 0> is 2>
assert<n.sqrt<0.000225, 2> is 0.02>
assert<n.sqrt<0.000625, 2> is 0.02>
assert<n.sqrt<0, 2> is 0>
assert<n.sqrt<152415787532388367501905199875019052100> is 12345678901234567890>
assert<n.round is round>
)");
            s.reject("n.sqrt<-1>", "non-negative"); s.reject("n.sqrt<1, -1>", "non-negative int");
            s.reject("n.pow<2, -1>", "non-negative int"); s.reject("n.pow<2, 1.5>", "non-negative int");
            s.reject("n.clamp<0, 5, 1>", "minimum <= maximum");
            s.reject("n.sum<list<1, \"2\">>", "Expected a number");
            s.reject("n.mean<list<>>", "non-empty");
            s.reject("n.abs<>", "number of arguments"); s.reject("n.sqrt<none>", "Expected a number");
        }},
        {"Unicode text positions, slices, case and whitespace", [] {
            Session s; s.run(R"(
use strings as s
assert<s.length<"Æøå🐟"> is 4>
assert<s.slice<"Æøå🐟", 2, 2> is "øå">
assert<s.slice<"Æøå🐟", 4> is "🐟">
assert<s.slice<"Æøå🐟", 5> is "">
assert<s.slice<"", 1, 0> is "">
assert<s.find<"Æøå🐟", "🐟"> is 4>
assert<s.find<"Æøå", "x"> is none>
assert<s.find<"", ""> is 1>
assert<s.contains<"rød", "ø">>
assert<s.lower<"ÆØÅÄÖÉ ÞÐ 🐟"> is "æøåäöé þð 🐟">
assert<s.upper<"æøåäöé þð ß 🐟"> is "ÆØÅÄÖÉ ÞÐ SS 🐟">
assert<s.trim<" \t\r\n Hei \n"> is "Hei">
assert<s.trim<" \t"> is "">
assert<s.replace<"bananana", "nana", "x"> is "baxna">
assert<s.replace<"blå", "å", "æø"> is "blæø">
var parts = s.split<"a,,b,", ",">
assert<length<parts> is 4>
assert<parts.2 is "">
assert<parts.4 is "">
assert<s.join<parts, "|"> is "a||b|">
assert<s.join<list<>, ","> is "">
assert<length<s.split<"", ",">> is 1>
)");
            s.reject("s.slice<\"abc\", 0>", "Indices start at 1");
            s.reject("s.slice<\"abc\", 2, 3>", "supported range");
            s.reject("s.split<\"abc\", \"\">", "non-empty");
            s.reject("s.replace<\"abc\", \"\", \"x\">", "non-empty");
            s.reject("s.join<list<1>, \",\">", "list of text");
            s.reject("s.length<42>", "needs text");
            s.reject("append<parts, 1>", "Expected text");
        }},
        {"Norwegian identifiers and explicit decimal input and display", [] {
            Session s; s.run(R"(
use numbers as tall
var øre = 0.01
var blåbær = 3
var Åse = "Hei"
assert<øre * 100 is 1>
assert<blåbær is 3>
assert<Åse is "Hei">
assert<tall.parse<" 0,01 ", ","> is øre>
assert<tall.parse<"-123456789012345678901234,56", ","> is -123456789012345678901234.56>
assert<tall.parse<"2.355"> is 2.355>
assert<type<tall.parse<"1">> is "dec">
assert<tall.format<øre, 2, ","> is "0,01">
assert<tall.format<2.345, 2, ","> is "2,34">
assert<tall.format<2.355, 2, ","> is "2,36">
assert<tall.format<-0.004, 2, ","> is "0,00">
assert<tall.format<9.999, 2> is "10.00">
assert<tall.format<-2.5, 0, ","> is "-2">
assert<tall.format<123456789012345678901234, 2, ","> is "123456789012345678901234,00">
var komma = 0,01
assert<type<komma> is "list">
)");
            s.reject("tall.parse<\"1.234,56\", \",\">", "grouping separators");
            s.reject("tall.parse<\"1,2,3\", \",\">", "Expected numeric text");
            s.reject("tall.parse<\"1,2\">", "Expected numeric text");
            s.reject("tall.parse<\"1\", \";\">", "Decimal separator");
            s.reject("tall.format<1, -1>", "non-negative int");
            s.reject("tall.parse<none>", "needs text");
        }},
        {"list copies, identity equality, stable sort and typed lists", [] {
            Session s; s.run(R"(
use lists as l
var source as list of dec = 3, 1, 2
var sorted = l.sort<source>
assert<source.1 is 3>
assert<sorted.1 is 1>
assert<type<sorted.1> is "dec">
l.append<sorted, 4>
assert<l.length<source> is 3>
assert<l.remove<sorted, 4> is 4>
var backwards = l.reverse<source>
assert<backwards.1 is 2>
var part = l.slice<source, 2, 2>
assert<part.1 is 1>
assert<l.length<l.slice<source, 4>> is 0>
assert<l.length<l.sort<list<>>> is 0>
assert<l.find<source, 1> is 2>
assert<l.find<source, "1"> is none>
assert<l.contains<source, 2>>
assert<l.contains<source, 9> is false>
var object = obj<score = 1>
var original = list<object>
var copied = l.copy<original>
assert<copied is not original>
assert<copied.1 is object>
copied.1.score = 9
assert<original.1.score is 9>
var words = l.sort<list<"ø", "z", "a", "æ", "å">>
assert<words.1 is "a">
assert<words.3 is "å">
var mixed = l.sort<list<2.0, 1, 2>>
assert<type<mixed.2> is "dec">
assert<type<mixed.3> is "int">
)");
            s.reject("l.append<sorted, \"wrong\">", "Expected dec");
            s.reject("l.sort<list<1, \"2\">>", "all numbers or all text");
            s.reject("l.sort<list<true>>", "all numbers or all text");
            s.reject("l.slice<source, 0>", "Indices start at 1");
            s.reject("l.length<\"abc\">", "needs a list");
        }},
        {"seeded random state, arbitrary ranges and copy semantics", [] {
            Session s; s.run(R"(
use random as r
use random as other
use lists as l
r.seed<123>
var first = r.integer<-1000000000000000000000, 1000000000000000000000>
var second = r.integer<1, 6>
other.seed<123>
assert<other.integer<-1000000000000000000000, 1000000000000000000000> is first>
assert<other.integer<1, 6> is second>
assert<r.integer<-7, -7> is -7>
var seen = list<>
repeat 200 times:
    var roll = r.integer<1, 6>
    assert<roll >= 1 and roll <= 6>
    if l.contains<seen, roll> is false:
        l.append<seen, roll>
    .
.
assert<length<seen> is 6>
var names = "Arne", "Siri", "Åse"
assert<l.contains<names, r.choice<names>>>
var shuffled = r.shuffle<names>
assert<length<shuffled> is 3>
assert<names.1 is "Arne">
for person in names:
    assert<l.contains<shuffled, person>>
.
assert<length<r.shuffle<list<>>> is 0>
)");
            s.reject("r.integer<2, 1>", "minimum <= maximum");
            s.reject("r.integer<1.0, 2>", "int bounds");
            s.reject("r.choice<list<>>", "non-empty");
            s.reject("r.seed<1.5>", "needs an int");
            s.reject("r.integer = 2", "read-only");
        }},
        {"UTF-8 files and paths relative to the defining source", [] {
            file(root / "save.slang", "lib save:\nuse files\npublic func Save <>:\nfiles.write<\"saved.txt\", \"saved\">\n.\n.");
            file(root / "sub" / "writer.slang", "lib writer:\nuse files\npublic func Save <>:\nfiles.write<\"local.txt\", \"local\">\n.\n.");
            Session s; s.run(R"(
use files as f
use strings as s
f.write<"Å lære.txt", "Første linje\n">
f.append<"Å lære.txt", "Andre linje 🐟\n">
assert<f.exists<"Å lære.txt">>
assert<f.read<"Å lære.txt"> is "Første linje\nAndre linje 🐟\n">
assert<length<s.split<f.read<"Å lære.txt">, "\n">> is 3>
f.write<"Å lære.txt", "Ny tekst">
assert<f.read<"Å lære.txt"> is "Ny tekst">
assert<f.exists<"definitely-not-present.txt"> is false>
use "save.slang" as save
use "sub/writer.slang" as writer
save.Save<>
writer.Save<>
assert<f.read<"saved.txt"> is "saved">
assert<f.read<"sub/local.txt"> is "local">
)");
            s.reject("f.write<\"Å lære.txt\", 3>", "needs text");
            s.run("assert<f.read<\"Å lære.txt\"> is \"Ny tekst\">");
            s.reject("f.read<\"definitely-not-present.txt\">", "Cannot read file");
            s.reject("f.read<\"sub\">", "regular file");
            s.reject("f.write<\"sub\", \"x\">", "regular file");
            s.reject("f.read<\"\">", "non-empty");
            file(root / "bom.txt", "\xef\xbb\xbfHello\r\n");
            s.run("assert<f.read<\"bom.txt\"> is \"Hello\\r\\n\">");
            file(root / "invalid.txt", std::string("\xc0\xaf", 2));
            s.reject("f.read<\"invalid.txt\">", "valid UTF-8");
            file(root / "large.txt", std::string(16 * 1024 * 1024 + 1, 'x'));
            s.reject("f.read<\"large.txt\">", "16 MiB");
        }},
        {"monotonic time and interruptible waits", [] {
            Session s; s.run("use time as t\nvar start = t.seconds<>\nt.sleep<0.015>\nassert<t.seconds<> >= start + 0.01>\nt.sleep<0>");
            s.reject("t.sleep<-1>", "between 0 and 86400");
            s.reject("t.sleep<86401>", "between 0 and 86400");
            s.reject("t.sleep<\"1\">", "Expected a number");
            bool stopped = false; int polls = 0;
            std::istringstream input; std::ostringstream output;
            Runtime runtime(input, output, [&] { return stopped; });
            RuntimeHost host; host.poll = [&] { if (++polls >= 4) stopped = true; }; runtime.set_host(host);
            const auto start = std::chrono::steady_clock::now();
            const std::string code = "use time\ntime.sleep<2>";
            try { runtime.execute(parse(code), false, code); throw std::runtime_error("sleep ignored interrupt"); }
            catch (const Diagnostic& error) { require(error.code == "R190", error.what()); }
            require(std::chrono::steady_clock::now() - start < std::chrono::seconds(1), "sleep did not stop promptly");
        }},
        {"library results and aliases survive garbage collection and reset", [] {
            Session s; s.run(R"(
use strings as s
use lists as l
use random as r
var words = s.split<"Æ,Ø,Å", ",">
var copied = l.copy<words>
repeat 1500 times:
    var temporary = r.shuffle<s.split<"a,b,c", ",">>
.
assert<s.join<copied, ""> is "ÆØÅ">
)");
            s.runtime.reset(); s.run("use strings\nassert<strings.upper<\"æøå\"> is \"ÆØÅ\">");
        }},
    };
    int failed = 0;
    for (const auto& [name, test] : tests) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& e) { ++failed; std::cerr << "FAIL " << name << ": " << e.what() << '\n'; }
    }
    return failed ? 1 : 0;
}
