#include "slang/ast.hpp"
#include <initializer_list>

namespace slang {
namespace {
template<class... Ts> struct Overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> Overloaded(Ts...) -> Overloaded<Ts...>;
using Field = std::pair<std::string_view, std::string>;

std::string object(std::string_view type, std::initializer_list<Field> fields = {}) {
    std::string out = "{\"type\":" + json_string(type);
    for (const auto& [key, value] : fields) out += "," + json_string(key) + ":" + value;
    return out + "}";
}
template<class Range, class Fn>
std::string array(const Range& range, Fn encode) {
    std::string out = "[";
    bool comma = false;
    for (const auto& value : range) {
        if (comma) out += ',';
        comma = true;
        out += encode(value);
    }
    return out + "]";
}
std::string nodes(const Nodes& values) { return array(values, ast_json); }
std::string parameters(const std::vector<Parameter>& values) {
    return array(values, [](const Parameter& p) {
        return "{\"name\":" + json_string(p.name) + ",\"annotation\":" + ast_json(p.annotation) + "}";
    });
}
std::string boolean(bool value) { return value ? "true" : "false"; }
} // namespace

std::string json_string(std::string_view value) {
    constexpr char hex[] = "0123456789abcdef";
    std::string out = "\"";
    for (const unsigned char c : value) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c < 0x20) {
            out += "\\u00";
            out += hex[c >> 4];
            out += hex[c & 15];
        } else out += static_cast<char>(c);
    }
    return out + '"';
}

std::string ast_json(const NodePtr& node) {
    if (!node) return "null";
    return std::visit(Overloaded{
        [](const Program& x) { return object("Program", {{"body", nodes(x.body)}}); },
        [](const Library& x) { return object("Library", {{"name", json_string(x.name)}, {"body", nodes(x.body)}}); },
        [](const Public& x) { return object("Public", {{"declaration", ast_json(x.declaration)}}); },
        [](const Import& x) { return object("Import", {{"path", json_string(x.path)}, {"alias", json_string(x.alias)}, {"bundled", boolean(x.bundled)}}); },
        [](const Literal& x) {
            const auto kind = x.kind == LiteralKind::Integer ? "int" : x.kind == LiteralKind::Decimal ? "dec" :
                x.kind == LiteralKind::Text ? "text" : x.kind == LiteralKind::Boolean ? "bool" : "none";
            return object("Literal", {{"kind", json_string(kind)}, {"value", x.kind == LiteralKind::None ? "null" :
                x.kind == LiteralKind::Boolean ? x.value : json_string(x.value)}});
        },
        [](const Name& x) { return object("Name", {{"name", json_string(x.name)}}); },
        [](const TypeName& x) { return object("TypeName", {{"name", json_string(x.name)}, {"element", ast_json(x.element)}}); },
        [](const Variable& x) { return object("VariableDeclaration", {{"name", json_string(x.name)}, {"annotation", ast_json(x.annotation)}, {"value", ast_json(x.value)}}); },
        [](const Function& x) { return object(x.named ? "FunctionDeclaration" : "FunctionExpression", {{"name", x.named ? json_string(x.name) : "null"}, {"parameters", parameters(x.parameters)}, {"body", nodes(x.body)}}); },
        [](const Object& x) { return object("ObjectDeclaration", {{"name", json_string(x.name)}, {"fields", parameters(x.fields)}}); },
        [](const Conditional& x) {
            return object("If", {{"branches", array(x.branches, [](const Branch& b) {
                return "{\"condition\":" + ast_json(b.condition) + ",\"body\":" + nodes(b.body) + "}";
            })}, {"otherwise", x.otherwise ? nodes(*x.otherwise) : "null"}});
        },
        [](const While& x) { return object("While", {{"condition", ast_json(x.condition)}, {"body", nodes(x.body)}}); },
        [](const Repeat& x) { return object("Repeat", {{"count", ast_json(x.count)}, {"body", nodes(x.body)}}); },
        [](const ForEach& x) { return object("ForEach", {{"variable", json_string(x.variable)}, {"values", ast_json(x.values)}, {"body", nodes(x.body)}}); },
        [](const ForRange& x) { return object("ForRange", {{"variable", json_string(x.variable)}, {"from", ast_json(x.from)}, {"to", ast_json(x.to)}, {"step", ast_json(x.step)}, {"body", nodes(x.body)}}); },
        [](const Return& x) { return object("Return", {{"value", ast_json(x.value)}}); },
        [](const Break&) { return object("Break"); },
        [](const Continue&) { return object("Continue"); },
        [](const Assignment& x) { return object("Assignment", {{"target", ast_json(x.target)}, {"value", ast_json(x.value)}}); },
        [](const ExpressionStatement& x) { return object("ExpressionStatement", {{"expression", ast_json(x.expression)}}); },
        [](const ListLiteral& x) { return object("ListLiteral", {{"items", nodes(x.items)}}); },
        [](const Call& x) {
            return object("Call", {{"callee", ast_json(x.callee)}, {"arguments", array(x.arguments, [](const Argument& a) {
                return "{\"name\":" + (a.name ? json_string(*a.name) : "null") + ",\"value\":" + ast_json(a.value) + "}";
            })}});
        },
        [](const Member& x) { return object("Member", {{"object", ast_json(x.object)}, {"name", json_string(x.name)}}); },
        [](const Index& x) { return object("Index", {{"object", ast_json(x.object)}, {"index", ast_json(x.index)}, {"computed", boolean(x.computed)}}); },
        [](const Conversion& x) { return object("Conversion", {{"target", json_string(x.target)}, {"value", ast_json(x.value)}}); },
        [](const Group& x) { return object("Group", {{"value", ast_json(x.value)}}); },
        [](const Unary& x) { return object("Unary", {{"operator", json_string(x.op)}, {"prefix", "true"}, {"value", ast_json(x.value)}}); },
        [](const Update& x) { return object("Update", {{"operator", json_string(x.op)}, {"prefix", boolean(x.prefix)}, {"value", ast_json(x.value)}}); },
        [](const Binary& x) { return object("Binary", {{"operator", json_string(x.op)}, {"left", ast_json(x.left)}, {"right", ast_json(x.right)}}); }
    }, node->data);
}
} // namespace slang
