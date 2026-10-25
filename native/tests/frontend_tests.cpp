#include "slang/parser.hpp"
#include "slang/lexer.hpp"
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace slang;
void require(bool condition, std::string_view message = "assertion failed") {
    if (!condition) throw std::runtime_error(std::string(message));
}
void reject(std::string_view source, std::string_view message) {
    try { (void)parse(source); }
    catch (const Diagnostic& e) {
        require(std::string_view(e.what()).find(message) != std::string_view::npos, e.what());
        return;
    }
    throw std::runtime_error("Expected rejection: " + std::string(source.substr(0, 80)));
}
NodePtr expression(std::string_view text) {
    auto program = parse(text);
    return get<ExpressionStatement>(get<Program>(program)->body.at(0))->expression;
}
NodePtr value(std::string_view text) {
    auto program = parse("var result = " + std::string(text));
    return get<Variable>(get<Program>(program)->body.at(0))->value;
}
std::string utf8(std::u8string_view text) {
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}
} // namespace

int main() {
    const std::vector<std::pair<std::string, std::function<void()>>> tests = {
        {"English and Norwegian normalize identically", [] {
            const auto english = parse("func Add <a as int, b as text>:\nret text(a) + b\n.\nif true and 1 is not none:\nvar i = 1\n.");
            const auto norsk = parse("funk Add <a som heltall, b som tekst>:\nretur tekst(a) + b\n.\nhvis sann og 1 er ikke ingen:\nvar i = 1\n.");
            require(ast_json(english) == ast_json(norsk));
            require(canonical_keyword("fra") == "from");
            require(canonical_keyword("til") == "to");
            require(canonical_keyword("i").empty());
        }},
        {"indentation does not affect syntax", [] {
            require(ast_json(parse("while active:\n if ready:\n  count++\n .\n.")) ==
                ast_json(parse("while active:\nif ready:\ncount++\n.\n.")));
        }},
        {"numbers preserve digits", [] {
            const std::string integer = "12345678901234567890123456789012345678901234567890";
            const std::string decimal = "123456789012345678901234567890.123456789000";
            require(get<Literal>(value(integer))->value == integer);
            require(get<Literal>(value(decimal))->value == decimal);
        }},
        {"nested angle calls", [] {
            const auto call = expression("Add<2, Multiply<3, 4>>");
            const auto& args = get<Call>(call)->arguments;
            require(args.size() == 2);
            require(get<Name>(get<Call>(args[1].value)->callee)->name == "Multiply");
        }},
        {"comparison arguments", [] {
            for (const auto* op : {">", ">=", "<=", "is", "is not"}) {
                auto call = expression(std::string("check<a ") + op + " b>");
                require(get<Binary>(get<Call>(call)->arguments[0].value)->op == op);
            }
            require(get<Binary>(expression("Add<2, 3> > 4"))->op == ">");
        }},
        {"angle ambiguity requires grouping", [] {
            reject("check<a < b>", "Ambiguous angle brackets");
            reject("f<a > +b> + c", "Ambiguous angle brackets");
            (void)parse("check<(a < b)>\nf<(a > +b)> + c\n(f<a> + b) > +c");
        }},
        {"multiline expressions", [] {
            require(get<Call>(value("Add<\n2,\nMultiply<3, 4>\n>"))->arguments.size() == 2);
            require(get<Binary>(get<Group>(value("(1 +\n2)"))->value)->op == "+");
            reject("var x = 1 +\n2", "complete expression");
        }},
        {"list literals and explicit lists", [] {
            require(get<ListLiteral>(value("1, 2, 3"))->items.size() == 3);
            require(get<Literal>(value("1")) != nullptr);
            require(get<Call>(value("list<>"))->arguments.empty());
            require(get<Call>(value("list<1>"))->arguments.size() == 1);
            require(get<Call>(expression("process<list<1, 2, 3>>"))->arguments.size() == 1);
        }},
        {"objects and field initializers", [] {
            const auto ast = parse("obj Animal:\nlegs as int\nfurry as bool\nname as text\n.\nvar dog = Animal<4, true, \"Rex\">");
            require(get<Object>(get<Program>(ast)->body[0])->fields.size() == 3);
            const auto named = value("obj<age = 1, legs = 2, name = \"Arne\">");
            require(get<Call>(named)->arguments[2].name == "name");
            require(!get<Call>(value("obj<1, 2, \"Arne\">"))->arguments[0].name);
            reject("var o = obj<1, name = \"Arne\">", "complete expression");
        }},
        {"fixed and computed indices", [] {
            require(get<Literal>(get<Index>(value("a.1"))->index)->value == "1");
            require(get<Index>(get<Index>(value("a.1.2"))->object) != nullptr);
            require(get<Binary>(get<Index>(value("a.<i + 1>"))->index)->op == "+");
            require(get<Member>(value("dog.name"))->name == "name");
            reject("a.0", "Indices start at 1");
            reject("a.000", "Indices start at 1");
            (void)parse("a.<i> = 99\na.<i>++");
        }},
        {"prefix and postfix updates", [] {
            auto ast = value("a++ + ++a");
            require(!get<Update>(get<Binary>(ast)->left)->prefix);
            require(get<Update>(get<Binary>(ast)->right)->prefix);
            require(get<Update>(value("--a"))->prefix);
            require(!get<Update>(value("a--"))->prefix);
            reject("1++", "complete expression");
            reject("1 = 2", "Assignment needs");
        }},
        {"precedence and associativity", [] {
            require(get<Binary>(get<Binary>(value("2 + 3 * 4"))->right)->op == "*");
            require(get<Binary>(get<Binary>(value("8 - 3 - 1"))->left)->op == "-");
            require(get<Binary>(get<Binary>(value("a or b and c"))->right)->op == "and");
            require(get<Binary>(get<Unary>(value("not a is b"))->value)->op == "is");
            require(get<Member>(get<Unary>(value("-dog.legs"))->value) != nullptr);
            reject("a < b < c", "complete expression");
        }},
        {"functions and conversions", [] {
            auto ast = parse("func Add <a as int, b as text>:\nret text(a) + b\n.");
            const auto* fn = get<Function>(get<Program>(ast)->body[0]);
            require(get<TypeName>(fn->parameters[0].annotation)->name == "int");
            const auto* sum = get<Binary>(get<Return>(fn->body[0])->value);
            require(get<Conversion>(sum->left)->target == "text");
            reject("print(\"Hello\")", "complete expression");
        }},
        {"closures and anonymous functions", [] {
            auto ast = parse("func MakeCounter <>:\nvar count = 0\nfunc Next <>:\ncount++\nret count\n.\nret Next\n.");
            const auto* outer = get<Function>(get<Program>(ast)->body[0]);
            require(get<Function>(outer->body[1]) != nullptr);
            require(get<Name>(get<Return>(outer->body[2])->value)->name == "Next");
            require(!get<Function>(value("func <x>:\nret x * 2\n."))->named);
        }},
        {"loop forms and condition branches", [] {
            auto ast = parse("for i from 5 to 1 step -1:\ncontinue\n.\nfor v in 1, 2:\nbreak\n.\nrepeat 3 times:\nprint<1>\n.\nif a:\nret 1\nelse if b:\nret 2\nelse:\nret 3\n.");
            const auto& body = get<Program>(ast)->body;
            require(get<ForRange>(body[0]) != nullptr);
            require(get<ForEach>(body[1]) != nullptr);
            require(get<Repeat>(body[2]) != nullptr);
            require(get<Conditional>(body[3])->branches.size() == 2);
            require(get<Conditional>(body[3])->otherwise->size() == 1);
        }},
        {"contextual eller and stopp", [] {
            const auto english = parse("while true:\nif a or b and c:\nif d:\nprint<1>\nelse:\nprint<2>\n.\nelse if d or e:\nprint<3>\nelse:\nbreak\n.\n.");
            const auto norsk = parse("mens sann:\nhvis a eller b og c:\nhvis d:\nprint<1>\neller:\nprint<2>\n.\neller hvis d eller e:\nprint<3>\neller:\nstopp\n.\n.");
            require(ast_json(english) == ast_json(norsk));
            const auto* outer = get<Conditional>(get<While>(get<Program>(norsk)->body[0])->body[0]);
            const auto* condition = get<Binary>(outer->branches[0].condition);
            require(condition->op == "or");
            require(get<Binary>(condition->right)->op == "and");
            require(get<Conditional>(outer->branches[0].body[0])->otherwise->size() == 1);
            require(get<Break>(outer->otherwise->at(0)) != nullptr);
            require(ast_json(value("check<a eller b>")) == ast_json(value("check<a or b>")));
            require(ast_json(value("(a\neller b)")) == ast_json(value("(a\nor b)")));
            require(get<Literal>(value("\"eller stopp\""))->value == "eller stopp");
            require(canonical_keyword("stopp") == "break");
            require(canonical_keyword("ellers").empty());
            require(canonical_keyword("bryt").empty());
        }},
        {"English else and or remain distinct", [] {
            reject("var x = a else b", "complete expression");
            reject("if a:\nprint<1>\nor:\nprint<2>\n.", "complete expression");
            reject("eller:\nprint<1>\n.", "complete expression");
        }},
        {"equality diagnostics", [] {
            reject("if x = 1:\nret x\n.", "Use 'is'");
            reject("x == 1", "Use 'is'");
            reject("x != 1", "Use 'is not'");
        }},
        {"block errors", [] {
            reject("while active:\ncount++", "opened on line 1 needs a closing period");
            reject("while active: count++\n.", "next line");
            reject("while active:\ncount++ .", "complete expression");
            reject(".", "Unexpected block ending");
        }},
        {"Unicode identifiers and text stay unchanged", [] {
            auto ast = parse(utf8(u8"var blåbær = \"hvis func sann\"\nvar 𐐀 = 2"));
            const auto& body = get<Program>(ast)->body;
            require(get<Variable>(body[0])->name == utf8(u8"blåbær"));
            require(get<Literal>(get<Variable>(body[0])->value)->value == "hvis func sann");
            require(get<Variable>(body[1])->name == utf8(u8"𐐀"));
            require(get<Literal>(expression("\"while\""))->value == "while");
            reject("1 \"is\" 1", "complete expression");
        }},
        {"Unicode validation and escapes", [] {
            require(get<Literal>(value("\"\\u00e5\\n\\ud83d\\ude00\""))->value == utf8(u8"å\n😀"));
            reject("\"\\ud800\"", "low surrogate");
            reject("\"\\udc00\"", "low surrogate");
            reject("\"\\q\"", "Invalid text escape");
            reject("\"unfinished", "Unterminated text");
            reject("var \xc0\xaf = 1", "Invalid UTF-8");
            reject("// \xff", "Invalid UTF-8");
        }},
        {"source spans use Unicode columns and byte offsets", [] {
            const auto source = utf8(u8"var blå = \"å\"\nblå++");
            const auto tokens = tokenize(source);
            require(tokens[1].span.begin.column == 5);
            require(tokens[1].span.end.column == 8);
            require(tokens[1].span.end.offset == 8);
            const auto ast = parse(source);
            const auto& body = get<Program>(ast)->body;
            require(body[1]->span.begin.line == 2);
            require(body[1]->span.end.column == 6);
        }},
        {"comments, BOM, CRLF, and text containing slashes", [] {
            auto ast = parse("\xef\xbb\xbfhvis sann:\r\nprint<\"https://example.test\">\r\n. // done\r\n");
            require(get<Conditional>(get<Program>(ast)->body[0]) != nullptr);
        }},
        {"many drawing calls are separate statements, not nested syntax", [] {
            std::string source;
            for (int i = 0; i < 200; ++i) source += "g.circle<10, 20, 5>\n";
            require(get<Program>(parse(source))->body.size() == 200);
            const auto multiline = parse("print<(1 >\n0)>\nprint<2>\n");
            require(get<Program>(multiline)->body.size() == 2);
        }},
        {"empty input is a program", [] {
            require(get<Program>(parse(""))->body.empty());
            require(get<Program>(parse("// comment\n"))->body.empty());
        }},
        {"deep syntax gets a diagnostic rather than overflowing the stack", [] {
            reject(std::string(1000, '(') + "1" + std::string(1000, ')'), "nesting is too deep");
        }},
        {"typed lists and empty returns", [] {
            auto ast = parse("var values as list of int = list<>\nfunc Stop <>:\nret\n.");
            const auto& body = get<Program>(ast)->body;
            require(get<TypeName>(get<TypeName>(get<Variable>(body[0])->annotation)->element)->name == "int");
            require(!get<Return>(get<Function>(body[1])->body[0])->value);
        }}
    };
    int failed = 0;
    for (const auto& [name, test] : tests) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& e) { ++failed; std::cerr << "FAIL " << name << ": " << e.what() << '\n'; }
    }
    std::cout << tests.size() - static_cast<std::size_t>(failed) << '/' << tests.size() << " native tests passed\n";
    return failed ? 1 : 0;
}
