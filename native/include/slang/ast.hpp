#pragma once

#include "slang/source.hpp"
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace slang {

struct Node;
// Immutable syntax nodes can be shared between candidate parses. Runtime values
// will have their own lifetime model; this is not the language's managed heap.
using NodePtr = std::shared_ptr<const Node>;
using Nodes = std::vector<NodePtr>;

enum class LiteralKind { Integer, Decimal, Text, Boolean, None };
struct Program { Nodes body; };
struct Library { std::string name; Nodes body; };
struct Public { NodePtr declaration; };
struct Import { std::string path; std::string alias; bool bundled; };
struct Literal { LiteralKind kind; std::string value; };
struct Name { std::string name; };
struct TypeName { std::string name; NodePtr element; };
struct Variable { std::string name; NodePtr annotation; NodePtr value; };
struct Parameter { std::string name; NodePtr annotation; Span span; };
struct Function { bool named; std::string name; std::vector<Parameter> parameters; Nodes body; };
struct Object { std::string name; std::vector<Parameter> fields; };
struct Branch { NodePtr condition; Nodes body; };
struct Conditional { std::vector<Branch> branches; std::optional<Nodes> otherwise; };
struct While { NodePtr condition; Nodes body; };
struct Repeat { NodePtr count; Nodes body; };
struct ForEach { std::string variable; NodePtr values; Nodes body; };
struct ForRange { std::string variable; NodePtr from; NodePtr to; NodePtr step; Nodes body; };
struct Return { NodePtr value; };
struct Break {};
struct Continue {};
struct Assignment { NodePtr target; NodePtr value; };
struct ExpressionStatement { NodePtr expression; };
struct ListLiteral { Nodes items; };
struct Argument { std::optional<std::string> name; NodePtr value; };
struct Call { NodePtr callee; std::vector<Argument> arguments; };
struct Member { NodePtr object; std::string name; };
struct Index { NodePtr object; NodePtr index; bool computed; };
struct Conversion { std::string target; NodePtr value; };
struct Group { NodePtr value; };
struct Unary { std::string op; NodePtr value; };
struct Update { std::string op; bool prefix; NodePtr value; };
struct Binary { std::string op; NodePtr left; NodePtr right; };

using NodeData = std::variant<Program, Library, Public, Import, Literal, Name, TypeName, Variable, Function,
    Object, Conditional, While, Repeat, ForEach, ForRange, Return, Break, Continue,
    Assignment, ExpressionStatement, ListLiteral, Call, Member, Index, Conversion,
    Group, Unary, Update, Binary>;

struct Node { Span span; NodeData data; };

template<class T, class... Args>
NodePtr make_node(Span span, Args&&... args) {
    return std::make_shared<const Node>(Node{span, T{std::forward<Args>(args)...}});
}

template<class T> const T* get(const NodePtr& node) {
    return node ? std::get_if<T>(&node->data) : nullptr;
}

// Stable, location-independent JSON is used by CLI inspection and conformance tests.
std::string ast_json(const NodePtr& node);
std::string json_string(std::string_view value);

} // namespace slang
