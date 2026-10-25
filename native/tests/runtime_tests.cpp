#include "slang/runtime.hpp"
#include "slang/parser.hpp"
#include "slang/number.hpp"
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
using namespace slang;
void require(bool condition, std::string message = "assertion failed") { if (!condition) throw std::runtime_error(std::move(message)); }
std::string run(std::string_view source, std::string input_text = {}) {
    std::istringstream input(std::move(input_text)); std::ostringstream output;
    Runtime runtime(input, output); runtime.execute(parse(source)); return output.str();
}
void output(std::string_view source, const std::string& expected) {
    auto actual = run(source); require(actual == expected, "Expected:\n" + expected + "Got:\n" + actual);
}
void reject(std::string_view source, std::string_view fragment) {
    try { (void)run(source); } catch (const Diagnostic& e) {
        require(std::string(e.what()).find(fragment) != std::string::npos, std::string("Wrong diagnostic: ") + e.what());
        require(e.span.begin.line > 0); return;
    }
    throw std::runtime_error("Expected rejection: " + std::string(source));
}
} // namespace

int main() {
    std::cout << std::unitbuf;
    const std::vector<std::pair<std::string, std::function<void()>>> tests = {
        {"arbitrary integers and exact decimals", [] {
            output("var x = 999999999999999999999999999999999999999999\nprint<x + 1>\nprint<x * x>\nprint<0.1 + 0.2>\nprint<19.95 * 3>\nprint<(x + 1) - x>",
                "1000000000000000000000000000000000000000000\n999999999999999999999999999999999999999998000000000000000000000000000000000000000001\n0.3\n59.85\n1\n");
        }},
        {"division and ties-to-even rounding", [] {
            output("print<1 / 3>\nprint<1 / 8>\nprint<round<2.345, 2>, round<2.355, 2>>\nprint<round<-2.345, 2>, round<-2.355, 2>>\nprint<divide<1, 8, 2>, divide<3, 8, 2>>\nprint<-7 % 3>\nprint<type<4 / 2>>", "0.3333333333333333333333333333\n0.125\n2.34 2.36\n-2.34 -2.36\n0.12 0.38\n-1\ndec\n");
            reject("print<1 / 0>", "zero"); reject("print<1 % 0>", "zero"); reject("print<1.5 % 2>", "two int");
        }},
        {"signed integer arithmetic against machine arithmetic in safe ranges", [] {
            for (int a = -150; a <= 150; a += 7) for (int b = -120; b <= 120; b += 11) {
                const auto x = Number::parse(std::to_string(a)), y = Number::parse(std::to_string(b));
                require(Number::add(x, y).str() == std::to_string(a + b));
                require(Number::subtract(x, y).str() == std::to_string(a - b));
                require(Number::multiply(x, y).str() == std::to_string(a * b));
                if (b) require(Number::remainder(x, y).str() == std::to_string(a % b));
            }
        }},
        {"strong binding types and explicit conversions", [] {
            output("var x as dec = 2\nx = 3\nprint<type<x>, x>\nprint<text(2) + \"3\">\nprint<int(\"42\"), dec(\"0.10\"), bool(0)>\nvar later = none\nlater = 8\nlater = none\nlater = 9\nprint<later>", "dec 3\n23\n42 0.1 false\n9\n");
            reject("var x = 2\nx = \"2\"", "Expected int");
            reject("print<2 + \"3\">", "Expected a number");
            reject("print<int(1.25)>", "fractional");
            reject("print<int(\"2x\")>", "numeric text");
            reject("var x = none\nx = 1\nx = \"no\"", "Expected int");
        }},
        {"Norwegian contextual branches and loop controls", [] {
            output("var x = 0\nmens sann:\nx++\nhvis x er 1 eller x er 2:\nfortsett\neller hvis x er 3:\nprint<x>\neller:\nstopp\n.\n.\nprint<x>", "3\n4\n");
        }},
        {"all loop forms and snapshot iteration", [] {
            output("var sum = 0\nfor i from 5 to 1 step -2:\nsum = sum + i\n.\nrepeat 3 times:\nsum++\n.\nvar a = 1, 2\nfor v in a:\nappend<a, 9>\nsum = sum + v\n.\nprint<sum, length<a>>", "15 4\n");
            reject("for i from 1 to 3 step 0:\n.\n", "non-zero");
            reject("repeat -1 times:\n.\n", "non-negative");
            reject("repeat 1.5 times:\n.\n", "non-negative");
        }},
        {"closures retain independent mutable captures", [] {
            output("func Counter <>:\nvar n = 0\nfunc Next <>:\nret ++n\n.\nret Next\n.\nvar a = Counter<>\nvar b = Counter<>\nprint<a<>, a<>, b<>, a<>>", "1 2 1 3\n");
        }},
        {"recursive and mutually recursive functions", [] {
            output("func Fact <n>:\nif n <= 1:\nret 1\n.\nret n * Fact<n - 1>\n.\nprint<Fact<30>>\nfunc Even <n>:\nif n is 0:\nret true\n.\nret Odd<n - 1>\n.\nfunc Odd <n>:\nif n is 0:\nret false\n.\nret Even<n - 1>\n.\nprint<Even<10>>", "265252859812191058636308480000000\ntrue\n");
        }},
        {"overloads and inferred parameter bindings", [] {
            output("func Add <a, b>:\nret a + b\n.\nfunc Add <a as int, b as text>:\nret text(a) + b\n.\nprint<Add<2, 3>, Add<2, \"3\">, Add<\"a\", \"b\">>", "5 23 ab\n");
            reject("func Add <a, b>:\nret a + b\n.\nAdd<2, \"3\">", "Expected a number");
            reject("func F <a>:\na = \"no\"\n.\nF<2>", "Expected int");
            reject("func F <a as int>:\nret a\n.\nF<\"no\">", "No overload");
            reject("func F <a as int, b>:\nret a\n.\nfunc F <a, b as int>:\nret b\n.\nF<1, 2>", "Ambiguous overload");
            reject("func F <x>:\n.\nfunc F <y>:\n.", "Duplicate overload");
        }},
        {"return type stays stable for each specialization", [] {
            reject("func F <x>:\nif x:\nret 1\n.\nret \"no\"\n.\nF<true>\nF<false>", "Expected int");
        }},
        {"fresh loop captures and lexical shadowing", [] {
            output("var fs = list<>\nfor i from 1 to 3:\nappend<fs, func <>:\nret i\n.\n>\n.\nprint<fs.1<>, fs.2<>, fs.3<>>\nvar x = 1\nif true:\nvar x = 2\nprint<x>\n.\nprint<x>", "1 2 3\n2\n1\n");
            reject("var x = 1\nif true:\nprint<x>\nvar x = 2\n.", "not been initialized");
        }},
        {"mutable lists, aliases and one-based access", [] {
            output("var a = 1, 2, 3\nvar b = a\nb.2 = 9\nvar i = 3\na.<i>++\nappend<a, \"text\">\nprint<a.2, a.3, a.4>\nprint<remove<a, 1>, length<b>>", "9 4 text\n1 3\n");
            reject("var a = list<>\nprint<a.1>", "supported range");
            reject("var a = 1, 2\nvar i = 0\nprint<a.<i>>", "start at 1");
        }},
        {"list annotations constrain mutation through aliases", [] {
            output("var a as list of int = list<>\nappend<a, 2>\na.1++\nprint<a.1>", "3\n");
            reject("var a = 1, 2\nvar b as list of int = a\nappend<a, \"no\">", "Expected int");
            reject("var a as list of int = 1, 2\na.1 = \"no\"", "Expected int");
        }},
        {"nominal objects, anonymous objects and typed fields", [] {
            output("obj Animal:\nlegs as int\nname as text\n.\nvar dog = Animal<4, \"Rex\">\nvar alias = dog\nalias.legs--\nvar o = obj<age = 1, name = \"Arne\">\nvar p = obj<1, 2, \"Arne\">\nprint<dog.name, dog.legs, o.name, p.3>", "Rex 3 Arne Arne\n");
            reject("obj Animal:\nlegs as int\n.\nvar a = Animal<\"four\">", "Expected int");
            reject("var o = obj<age = 1>\no.age = \"one\"", "Expected int");
            reject("var o = obj<age = 1>\nprint<o.name>", "no field");
        }},
        {"updates and calls evaluate left to right once", [] {
            output("var x = 1\nprint<x++, ++x, x--, --x, x>\nvar a = 10, 20\nvar i = 1\na.<i++>++\nprint<a.1, a.2, i>", "1 3 3 1 1\n11 20 2\n");
        }},
        {"short-circuiting and truthiness", [] {
            output("var x = 0\nprint<false and ++x, true or ++x, x>\nprint<bool(none), bool(0), bool(\"\"), bool(list<>), bool(obj<>)>\nprint<0 is false, none is 0, 1 is 1.0>", "false true 0\nfalse false false false true\nfalse false true\n");
        }},
        {"explicit errors stop callers and preserve session state and source", [] {
            std::istringstream in; std::ostringstream out; Runtime runtime(in, out);
            const std::string source = "lib validation:\npublic func Fail <message as text>:\nerror<message>\nprint<\"unreachable\">\n.\n.";
            runtime.execute(parse(source), false, source, "validation.slang");
            const std::string call = "var score = 1\nscore++\nvalidation.Fail<\"Invalid score: \" + text(score)>\nprint<\"unreachable caller\">";
            try { runtime.execute(parse(call), false, call, "caller.slang"); throw std::runtime_error("expected error"); }
            catch (const Diagnostic& e) {
                require(std::string(e.what()) == "Invalid score: 2");
                require(e.source_name == "validation.slang"); require(e.source_text == source);
                require(e.span.begin.line == 3); require(e.span.begin.column == 1);
            }
            require(out.str().empty());
            runtime.execute(parse("print<score>")); require(out.str() == "2\n");
            runtime.reset();
            try { runtime.execute(parse("error<\"Still available\">")); throw std::runtime_error("expected error"); }
            catch (const Diagnostic& e) { require(std::string(e.what()) == "Still available"); }
        }},
        {"explicit errors require exactly one text message", [] {
            reject("error<>", "wrong number of arguments");
            reject("error<\"a\", \"b\">", "wrong number of arguments");
            reject("error<42>", "needs a text message");
            reject("error<none>", "needs a text message");
            reject("error<\"\">", "");
            output("false and error<\"unreachable\">\ntrue or error<\"unreachable\">\nprint<\"ok\">", "ok\n");
        }},
        {"name validation happens before effects", [] {
            std::istringstream in; std::ostringstream out; Runtime runtime(in, out);
            try { runtime.execute(parse("print<\"wrong\">\nif false:\nprint<missing>\n.")); throw std::runtime_error("expected error"); }
            catch (const Diagnostic& e) { require(std::string(e.what()).find("Unknown name") != std::string::npos); }
            require(out.str().empty());
            runtime.execute(parse("var x = 1"));
            reject("var x = 1\nvar x = 2", "already declared");
            reject("var x as Missing = none", "Unknown type");
        }},
        {"control flow cannot cross function boundaries", [] {
            reject("ret 1", "inside a function"); reject("stopp", "inside a loop");
            reject("while true:\nfunc F <>:\nbreak\n.\n.", "inside a loop");
        }},
        {"persistent sessions, collection and reset", [] {
            std::istringstream in; std::ostringstream out; Runtime runtime(in, out);
            runtime.execute(parse("func Counter <>:\nvar x = 0\nret func <>:\nret ++x\n.\n.\nvar next = Counter<>"));
            runtime.execute(parse("next<>"), true); runtime.execute(parse("next<>"), true);
            runtime.execute(parse("var a = list<>\nappend<a, a>\nprint<a>"));
            runtime.execute(parse("print<a>"));
            require(out.str() == "1\n2\nlist<<cycle>>\nlist<<cycle>>\n");
            runtime.reset(); runtime.execute(parse("var next = 9\nprint<next>"));
            require(out.str().ends_with("9\n"));
        }},
        {"console input and EOF", [] {
            require(run("var name = input<\"Name? \">\nprint<\"Hello \" + name>\nprint<input<> is none>", "Ada\n") == "Name? Hello Ada\ntrue\n");
        }},
        {"closure diagnostics retain the defining source", [] {
            std::istringstream in; std::ostringstream out; Runtime runtime(in, out);
            const std::string source = "func F <>:\nret 1 / 0\n.";
            runtime.execute(parse(source), false, source, "definition.slang");
            try { runtime.execute(parse("F<>"), true, "F<>", "call.slang"); throw std::runtime_error("expected error"); }
            catch (const Diagnostic& e) { require(e.source_name == "definition.slang"); require(e.source_text == source); require(e.span.begin.line == 2); }
        }},
        {"duplicate overloads do not modify a persistent session", [] {
            std::istringstream in; std::ostringstream out; Runtime runtime(in, out);
            runtime.execute(parse("func F <x>:\nret x\n."));
            try { runtime.execute(parse("var bad = 1\nfunc F <y>:\nret y\n.")); throw std::runtime_error("expected error"); }
            catch (const Diagnostic& e) { require(std::string(e.what()).find("Duplicate overload") != std::string::npos); }
            runtime.execute(parse("var bad = 2\nprint<F<bad>>")); require(out.str() == "2\n");
        }},
        {"deep collection graphs are traced without native recursion", [] {
            std::istringstream in; std::ostringstream out; Runtime runtime(in, out);
            runtime.execute(parse("var a = list<>\nrepeat 2000 times:\na = list<a>\n.\nprint<length<a>>"));
            runtime.execute(parse("print<length<a>>")); require(out.str() == "1\n1\n");
        }},
        {"collection preserves temporary values and assignment targets", [] {
            output("func Churn <>:\nrepeat 1300 times:\nvar temporary = obj<list<1, 2>>\n.\nret 7\n.\nfunc First <a, b>:\nret a\n.\nvar a = obj<value = 1>\nfunc Drop <>:\na = none\nret Churn<>\n.\na.value = Drop<>\nvar listvalue = list<obj<42>, Churn<>>\nprint<listvalue.1.1>\nprint<First<obj<8>, Churn<>>.1>\nvar values = list<obj<9>, obj<10>>\nfor item in values:\nvalues = list<>\nChurn<>\nprint<item.1>\n.", "42\n8\n9\n10\n");
        }},
        {"instruction and recursion limits", [] {
            std::istringstream in; std::ostringstream out; Runtime runtime(in, out, {}, 30);
            try { runtime.execute(parse("while true:\n.")); throw std::runtime_error("expected limit"); }
            catch (const Diagnostic& e) { require(e.code == "R191"); }
            runtime.execute(parse("print<1>")); require(out.str() == "1\n");
            reject("func F <>:\nF<>\n.\nF<>", "call depth");
        }},
        {"interrupt callback", [] {
            std::istringstream in; std::ostringstream out; Runtime runtime(in, out, [] { return true; });
            try { runtime.execute(parse("print<1>")); throw std::runtime_error("expected interruption"); }
            catch (const Diagnostic& e) { require(e.code == "R190"); }
            require(out.str().empty());
        }},
        {"nested expressions in recursive functions are bounded", [] {
            const std::string grouped = std::string(15, '(') + "F<>" + std::string(15, ')');
            reject("func F <>:\nret " + grouped + "\n.\nF<>", "nesting limit");
        }}
    };
    int failures = 0;
    for (const auto& [name, test] : tests) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& e) { ++failures; std::cerr << "FAIL " << name << ": " << e.what() << '\n'; }
    }
    std::cout << tests.size() - static_cast<std::size_t>(failures) << '/' << tests.size() << " runtime tests passed\n";
    return failures ? 1 : 0;
}
