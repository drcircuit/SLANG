#include "slang/parser.hpp"
#include "slang/lexer.hpp"
#include <algorithm>
#include <functional>
#include <map>
#include <set>
#include <tuple>

namespace slang {
namespace {
bool type_name(std::string_view value) {
    return value == "int" || value == "dec" || value == "text" || value == "bool" || value == "list" || value == "obj";
}
int precedence(std::string_view op) {
    if (op == "or") return 10;
    if (op == "and") return 20;
    if (op == "is" || op == "is not" || op == "<" || op == ">" || op == "<=" || op == ">=") return 30;
    if (op == "+" || op == "-") return 40;
    if (op == "*" || op == "/" || op == "%") return 50;
    return -1;
}
bool assignable(const NodePtr& node) { return get<Name>(node) || get<Member>(node) || get<Index>(node); }
int binding_power(const NodePtr& node) {
    if (const auto* binary = get<Binary>(node)) return precedence(binary->op);
    if (const auto* unary = get<Unary>(node)) return unary->op == "not" ? 25 : 70;
    if (const auto* update = get<Update>(node)) return update->prefix ? 70 : 80;
    return 100;
}
bool callable_syntax(const NodePtr& node) {
    return get<Name>(node) || get<Member>(node) || get<Index>(node) || get<Call>(node) ||
        get<Group>(node) || get<Function>(node);
}

struct Candidate { NodePtr node; std::size_t pos; };
using Candidates = std::vector<Candidate>;
struct ArgumentCandidate { std::vector<Argument> args; std::size_t pos; };

class Parser {
public:
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)), token_limit_(tokens_.size()) {}
    NodePtr program();
private:
    std::vector<Token> tokens_;
    std::size_t token_limit_;
    std::size_t pos_ = 0;
    std::size_t depth_ = 0;
    std::size_t work_ = 0;
    std::map<std::tuple<std::size_t, int, bool, std::size_t>, Candidates> cache_;
    struct Depth {
        Parser& owner;
        explicit Depth(Parser& owner, std::size_t at) : owner(owner) {
            if (owner.depth_ >= 128) owner.fail("Syntax nesting is too deep for this frontend", at, "S121");
            ++owner.depth_;
        }
        ~Depth() { --owner.depth_; }
    };
    const Token& token(std::size_t pos) const { return tokens_[pos >= token_limit_ ? tokens_.size() - 1 : std::min(pos, tokens_.size() - 1)]; }
    const Token& current() const { return token(pos_); }
    bool is(std::string_view value, std::size_t pos) const {
        const auto& t = token(pos);
        return (t.kind == TokenKind::Keyword || t.kind == TokenKind::Symbol ||
            t.kind == TokenKind::Newline || t.kind == TokenKind::End) && t.value == value;
    }
    bool is(std::string_view value) const { return is(value, pos_); }
    bool is_else() const { return is("else") || is("else_or"); }
    [[noreturn]] void fail(std::string message, std::size_t at, std::string code = "S100") const {
        throw Diagnostic(std::move(code), std::move(message), token(at).span);
    }
    Token expect(std::string_view value) {
        if (!is(value)) fail("Expected '" + std::string(value) + "', found '" + current().value + "'", pos_);
        return tokens_[pos_++];
    }
    std::string name() {
        if (current().kind != TokenKind::Identifier) fail("Expected a name", pos_);
        return tokens_[pos_++].value;
    }
    std::size_t skip(std::size_t at, bool multiline) const {
        while (multiline && token(at).kind == TokenKind::Newline) ++at;
        return at;
    }
    void skip_lines() { pos_ = skip(pos_, true); }
    bool boundary(std::size_t at) const {
        return token(at).kind == TokenKind::Newline || token(at).kind == TokenKind::End;
    }
    Span span(std::size_t start, std::size_t end) const {
        return {token(start).span.begin, end > start ? token(end - 1).span.end : token(start).span.begin};
    }
    void end_statement() {
        if (!boundary(pos_)) fail("Expected the end of the line", pos_);
        skip_lines();
    }
    void open_block() {
        expect(":");
        if (current().kind != TokenKind::Newline) fail("Put the block's instructions on the next line", pos_);
        skip_lines();
    }
    void close_block(const Token& opening) {
        if (!is(".")) fail("The block opened on line " + std::to_string(opening.span.begin.line) + " needs a closing period", pos_);
        if (pos_ == 0 || token(pos_ - 1).kind != TokenKind::Newline) fail("A closing period must be on its own line", pos_);
        ++pos_;
        if (!boundary(pos_)) fail("A closing period must be on its own line", pos_);
    }
    void tick(std::size_t at) {
        if (++work_ > 200000) fail("Expression search is too complex; split it into smaller expressions", at, "S120");
    }
    Candidates unique(Candidates candidates) {
        Candidates result;
        std::set<std::pair<std::size_t, std::string>> seen;
        for (auto& c : candidates) {
            if (seen.emplace(c.pos, ast_json(c.node)).second) result.push_back(std::move(c));
            if (result.size() > 256) fail("Too many possible expression parses; use grouping parentheses", result.back().pos, "S120");
        }
        return result;
    }
    enum class Context { Program, Library, Block };
    Nodes body(bool stop_at_else = false, Context context = Context::Block);
    NodePtr statement(Context context = Context::Block);
    NodePtr parse_type();
    NodePtr function(bool named);
    NodePtr condition();
    NodePtr expression_until(const std::function<bool(std::size_t)>& ends, bool sequence = false);
    NodePtr expression_candidates_until(const std::function<bool(std::size_t)>& ends, bool sequence);
    Candidates sequences(std::size_t pos, bool multiline);
    Candidates expressions(std::size_t pos, int min, bool multiline);
    Candidates prefixes(std::size_t pos, bool multiline);
    Candidates grow(NodePtr node, std::size_t pos, int min, bool multiline);
    std::vector<ArgumentCandidate> arguments(std::size_t pos, bool named_allowed);
};

NodePtr Parser::program() {
    skip_lines();
    auto statements = body(false, Context::Program);
    if (current().kind != TokenKind::End) fail("Unexpected block ending", pos_);
    return make_node<Program>(span(0, pos_), std::move(statements));
}
Nodes Parser::body(bool stop_at_else, Context context) {
    Depth depth(*this, pos_);
    Nodes statements;
    skip_lines();
    while (current().kind != TokenKind::End && !is(".") && !(stop_at_else && is_else())) {
        statements.push_back(statement(context));
        end_statement();
    }
    return statements;
}
NodePtr Parser::parse_type() {
    Depth depth(*this, pos_);
    const auto start = pos_;
    if (current().kind != TokenKind::Identifier && !(current().kind == TokenKind::Keyword && type_name(current().value))) {
        fail("Expected a type", pos_);
    }
    auto type = tokens_[pos_++].value;
    if (!type_name(type)) while (is(".")) { ++pos_; type += "." + name(); }
    NodePtr element;
    if (type == "list" && is("of")) { ++pos_; element = parse_type(); }
    return make_node<TypeName>(span(start, pos_), std::move(type), element);
}
NodePtr Parser::function(bool named) {
    Depth depth(*this, pos_);
    const auto start = pos_;
    const auto opening = expect("func");
    auto function_name = named ? name() : std::string{};
    expect("<");
    std::vector<Parameter> parameters;
    skip_lines();
    if (!is(">")) {
        while (true) {
            skip_lines();
            const auto parameter_start = pos_;
            auto parameter_name = name();
            NodePtr annotation;
            if (is("as")) { ++pos_; annotation = parse_type(); }
            parameters.push_back({std::move(parameter_name), annotation, span(parameter_start, pos_)});
            skip_lines();
            if (!is(",")) break;
            ++pos_;
        }
    }
    expect(">");
    open_block();
    auto statements = body();
    close_block(opening);
    return make_node<Function>(span(start, pos_), named, std::move(function_name), std::move(parameters), std::move(statements));
}
NodePtr Parser::statement(Context context) {
    const auto start = pos_;
    const auto opening = current();
    // Cache entries contain immutable nodes and only serve the current statement.
    // Nested function parsing can share entries without changing their meaning.
    if (depth_ == 1) { cache_.clear(); work_ = 0; }
    if (is("lib")) {
        if (context != Context::Program) fail("lib must be at the top level of a file or console submission", pos_);
        ++pos_;
        auto library_name = name();
        open_block();
        auto statements = body(false, Context::Library);
        close_block(opening);
        return make_node<Library>(span(start, pos_), std::move(library_name), std::move(statements));
    }
    if (is("public")) {
        if (context != Context::Library) fail("public is only allowed directly inside lib", pos_);
        ++pos_;
        if (!(is("func") || is("obj")) || token(pos_ + 1).kind != TokenKind::Identifier) fail("public must precede a named func or obj declaration", pos_);
        auto declaration = statement(Context::Block);
        return make_node<Public>(span(start, pos_), declaration);
    }
    if (is("use")) {
        if (context == Context::Block) fail("use must be at the top level or directly inside lib", pos_);
        ++pos_;
        const bool bundled = current().kind != TokenKind::Text;
        std::string path, alias;
        if (bundled) { path = name(); alias = path; }
        else { path = tokens_[pos_++].value; expect("as"); alias = name(); }
        if (bundled && is("as")) { ++pos_; alias = name(); }
        return make_node<Import>(span(start, pos_), std::move(path), std::move(alias), bundled);
    }
    if (is("var")) {
        ++pos_;
        auto variable_name = name();
        NodePtr annotation;
        if (is("as")) { ++pos_; annotation = parse_type(); }
        expect("=");
        auto value = expression_until([&](auto p) { return boundary(p); }, true);
        return make_node<Variable>(span(start, pos_), std::move(variable_name), annotation, value);
    }
    if (is("func")) return function(true);
    if (is("obj") && token(pos_ + 1).kind == TokenKind::Identifier) {
        ++pos_;
        auto object_name = name();
        open_block();
        std::vector<Parameter> fields;
        while (!is(".") && current().kind != TokenKind::End) {
            const auto field_start = pos_;
            auto field_name = name();
            expect("as");
            auto annotation = parse_type();
            fields.push_back({std::move(field_name), annotation, span(field_start, pos_)});
            end_statement();
        }
        close_block(opening);
        return make_node<Object>(span(start, pos_), std::move(object_name), std::move(fields));
    }
    if (is("if")) {
        ++pos_;
        std::vector<Branch> branches;
        std::optional<Nodes> otherwise;
        while (true) {
            auto test = condition();
            open_block();
            branches.push_back({test, body(true)});
            if (!is_else()) break;
            ++pos_;
            if (is("if")) { ++pos_; continue; }
            open_block();
            otherwise = body();
            break;
        }
        close_block(opening);
        return make_node<Conditional>(span(start, pos_), std::move(branches), std::move(otherwise));
    }
    if (is("while")) {
        ++pos_;
        auto test = condition();
        open_block();
        auto statements = body();
        close_block(opening);
        return make_node<While>(span(start, pos_), test, std::move(statements));
    }
    if (is("repeat")) {
        ++pos_;
        auto count = expression_until([&](auto p) { return is("times", p); });
        expect("times");
        open_block();
        auto statements = body();
        close_block(opening);
        return make_node<Repeat>(span(start, pos_), count, std::move(statements));
    }
    if (is("for")) {
        ++pos_;
        auto variable = name();
        if (is("in")) {
            ++pos_;
            auto values = expression_until([&](auto p) { return is(":", p); }, true);
            open_block();
            auto statements = body();
            close_block(opening);
            return make_node<ForEach>(span(start, pos_), std::move(variable), values, std::move(statements));
        }
        expect("from");
        auto from = expression_until([&](auto p) { return is("to", p); });
        expect("to");
        auto to = expression_until([&](auto p) { return is("step", p) || is(":", p); });
        NodePtr step;
        if (is("step")) { ++pos_; step = expression_until([&](auto p) { return is(":", p); }); }
        open_block();
        auto statements = body();
        close_block(opening);
        return make_node<ForRange>(span(start, pos_), std::move(variable), from, to, step, std::move(statements));
    }
    if (is("ret")) {
        ++pos_;
        auto value = boundary(pos_) ? NodePtr{} : expression_until([&](auto p) { return boundary(p); }, true);
        return make_node<Return>(span(start, pos_), value);
    }
    if (is("break")) { ++pos_; return make_node<Break>(span(start, pos_)); }
    if (is("continue")) { ++pos_; return make_node<Continue>(span(start, pos_)); }
    auto left = expression_until([&](auto p) { return is("=", p) || boundary(p); });
    if (is("=")) {
        if (!assignable(left)) fail("Assignment needs a variable, field, or list item", pos_);
        ++pos_;
        auto right = expression_until([&](auto p) { return boundary(p); }, true);
        return make_node<Assignment>(span(start, pos_), left, right);
    }
    return make_node<ExpressionStatement>(span(start, pos_), left);
}
NodePtr Parser::condition() {
    auto test = expression_until([&](auto p) { return is(":", p) || is("=", p); });
    if (is("=")) fail("Use 'is' (or 'er') to compare values; '=' assigns a value", pos_);
    return test;
}
NodePtr Parser::expression_until(const std::function<bool(std::size_t)>& ends, bool sequence) {
    // Try the first physical line before exploring multiline alternatives.
    // A completed call must not recursively speculate through later statements
    // by interpreting its closing '>' as a comparison with the next line.
    const auto previous_limit = token_limit_;
    auto line_end = pos_;
    while (line_end < previous_limit && tokens_[line_end].kind != TokenKind::Newline && tokens_[line_end].kind != TokenKind::End) ++line_end;
    if (line_end + 1 < previous_limit) {
        token_limit_ = line_end + 1;
        try {
            auto result = expression_candidates_until(ends, sequence);
            token_limit_ = previous_limit;
            return result;
        } catch (const Diagnostic& error) {
            token_limit_ = previous_limit;
            if (error.code == "S110") throw;
        } catch (...) { token_limit_ = previous_limit; throw; }
    }
    return expression_candidates_until(ends, sequence);
}
NodePtr Parser::expression_candidates_until(const std::function<bool(std::size_t)>& ends, bool sequence) {
    const auto start = pos_;
    auto candidates = sequence ? sequences(start, false) : expressions(start, 0, false);
    Candidates matches;
    for (auto& c : candidates) {
        if (!ends(c.pos)) continue;
        if (token(c.pos).kind == TokenKind::Newline) {
            auto next = c.pos;
            while (next < tokens_.size() && tokens_[next].kind == TokenKind::Newline) ++next;
            if (next < tokens_.size() && tokens_[next].kind == TokenKind::Symbol &&
                (tokens_[next].value == ">" || tokens_[next].value == ")" || tokens_[next].value == ",")) continue;
        }
        matches.push_back(std::move(c));
    }
    matches = unique(std::move(matches));
    if (matches.empty()) fail("Expected a complete expression", start);
    // A complete statement ends at its first valid boundary. Speculative
    // angle comparisons must not consume a following statement as an argument.
    const auto earliest = std::min_element(matches.begin(), matches.end(), [](const auto& a, const auto& b) { return a.pos < b.pos; })->pos;
    std::erase_if(matches, [&](const auto& candidate) { return candidate.pos != earliest; });
    if (matches.size() > 1) fail("Ambiguous angle brackets: use parentheses to group the intended expression", start, "S110");
    pos_ = matches[0].pos;
    return matches[0].node;
}
Candidates Parser::sequences(std::size_t pos, bool multiline) {
    Candidates results;
    std::function<void(const Nodes&, std::size_t)> visit = [&](const Nodes& items, std::size_t at) {
        Depth depth(*this, at);
        tick(at);
        for (const auto& c : expressions(at, 0, multiline)) {
            auto all = items;
            all.push_back(c.node);
            auto node = all.size() == 1 ? all.front() : make_node<ListLiteral>(span(pos, c.pos), all);
            results.push_back({node, c.pos});
            const auto next = skip(c.pos, multiline);
            if (is(",", next)) visit(all, next + 1);
        }
    };
    visit({}, pos);
    return unique(std::move(results));
}
Candidates Parser::expressions(std::size_t pos, int min, bool multiline) {
    Depth depth(*this, pos);
    pos = skip(pos, multiline);
    const auto key = std::tuple{pos, min, multiline, token_limit_};
    if (const auto found = cache_.find(key); found != cache_.end()) return found->second;
    tick(pos);
    Candidates result;
    for (const auto& c : prefixes(pos, multiline)) {
        auto grown = grow(c.node, c.pos, min, multiline);
        result.insert(result.end(), grown.begin(), grown.end());
    }
    result = unique(std::move(result));
    cache_[key] = result;
    return result;
}
Candidates Parser::prefixes(std::size_t pos, bool multiline) {
    Depth depth(*this, pos);
    tick(pos);
    const auto& t = token(pos);
    if (t.kind == TokenKind::Integer || t.kind == TokenKind::Decimal || t.kind == TokenKind::Text) {
        const auto kind = t.kind == TokenKind::Integer ? LiteralKind::Integer :
            t.kind == TokenKind::Decimal ? LiteralKind::Decimal : LiteralKind::Text;
        return {{make_node<Literal>(t.span, kind, t.value), pos + 1}};
    }
    if (is("true", pos) || is("false", pos) || is("none", pos)) {
        return {{make_node<Literal>(t.span, t.value == "none" ? LiteralKind::None : LiteralKind::Boolean, t.value), pos + 1}};
    }
    if (is("func", pos)) {
        if (!is("<", pos + 1)) return {};
        // Restore the statement cursor after parsing an anonymous function candidate.
        struct Restore { std::size_t& at; std::size_t old; ~Restore() { at = old; } } restore{pos_, pos_};
        pos_ = pos;
        auto node = function(false);
        return {{node, pos_}};
    }
    if (t.kind == TokenKind::Identifier || (t.kind == TokenKind::Keyword && type_name(t.value))) {
        if (type_name(t.value) && is("(", pos + 1)) {
            Candidates result;
            for (const auto& c : expressions(pos + 2, 0, true)) {
                const auto end = skip(c.pos, true);
                if (is(")", end)) result.push_back({make_node<Conversion>(span(pos, end + 1), t.value, c.node), end + 1});
            }
            return result;
        }
        return {{make_node<Name>(t.span, t.value), pos + 1}};
    }
    if (is("(", pos)) {
        Candidates result;
        for (const auto& c : expressions(pos + 1, 0, true)) {
            const auto end = skip(c.pos, true);
            if (is(")", end)) result.push_back({make_node<Group>(span(pos, end + 1), c.node), end + 1});
        }
        return result;
    }
    if (is("+", pos) || is("-", pos) || is("not", pos) || is("++", pos) || is("--", pos)) {
        Candidates result;
        const bool update = t.value == "++" || t.value == "--";
        for (const auto& c : expressions(pos + 1, t.value == "not" ? 25 : 70, multiline)) {
            if (update && !assignable(c.node)) continue;
            auto node = update ? make_node<Update>(span(pos, c.pos), t.value, true, c.node) :
                make_node<Unary>(span(pos, c.pos), t.value, c.node);
            result.push_back({node, c.pos});
        }
        return result;
    }
    return {};
}
std::vector<ArgumentCandidate> Parser::arguments(std::size_t pos, bool named_allowed) {
    Depth depth(*this, pos);
    std::vector<ArgumentCandidate> result;
    const auto first = skip(pos, true);
    if (is(">", first)) result.push_back({{}, first + 1});
    std::function<void(const std::vector<Argument>&, std::size_t, std::optional<bool>)> visit;
    visit = [&](const auto& args, std::size_t at, std::optional<bool> mode) {
        Depth argument_depth(*this, at);
        tick(at);
        at = skip(at, true);
        const bool named = named_allowed && token(at).kind == TokenKind::Identifier && is("=", at + 1);
        if (mode && *mode != named) return;
        const auto field = named ? std::optional<std::string>(token(at).value) : std::nullopt;
        for (const auto& c : expressions(named ? at + 2 : at, 0, true)) {
            const auto end = skip(c.pos, true);
            auto all = args;
            all.push_back({field, c.node});
            if (is(">", end)) result.push_back({all, end + 1});
            if (is(",", end)) visit(all, end + 1, named);
        }
    };
    visit({}, first, std::nullopt);
    return result;
}
Candidates Parser::grow(NodePtr node, std::size_t pos, int min, bool multiline) {
    Depth depth(*this, pos);
    tick(pos);
    Candidates results{{node, pos}};
    const auto next = skip(pos, multiline);
    const auto& t = token(next);
    const auto extend = [&](NodePtr value, std::size_t end) {
        auto extra = grow(std::move(value), end, min, multiline);
        results.insert(results.end(), extra.begin(), extra.end());
        if (results.size() > 1024) fail("Too many possible expression parses; use grouping parentheses", next, "S120");
    };
    const auto covering = [&](std::size_t end) { return Span{node->span.begin, token(end - 1).span.end}; };
    if (is("<", next) && min <= 90 && callable_syntax(node)) {
        const auto* callee = get<Name>(node);
        for (const auto& call : arguments(next + 1, callee && callee->name == "obj")) {
            extend(make_node<Call>(covering(call.pos), node, call.args), call.pos);
        }
    }
    if (is(".", next) && min <= 90 && binding_power(node) >= 90) {
        const auto& member = token(next + 1);
        if (member.kind == TokenKind::Identifier || member.kind == TokenKind::Keyword) extend(make_node<Member>(covering(next + 2), node, member.raw), next + 2);
        if (member.kind == TokenKind::Integer) {
            if (member.value.find_first_not_of('0') == std::string::npos) fail("Indices start at 1", next + 1);
            auto index = make_node<Literal>(member.span, LiteralKind::Integer, member.value);
            extend(make_node<Index>(covering(next + 2), node, index, false), next + 2);
        }
        if (is("<", next + 1)) {
            for (const auto& c : expressions(next + 2, 0, true)) {
                const auto end = skip(c.pos, true);
                if (is(">", end)) extend(make_node<Index>(covering(end + 1), node, c.node, true), end + 1);
            }
        }
    }
    if ((is("++", next) || is("--", next)) && min <= 80 && assignable(node)) {
        extend(make_node<Update>(covering(next + 1), t.value, false, node), next + 1);
    }
    auto op = (t.kind == TokenKind::Keyword || t.kind == TokenKind::Symbol) ? t.value : std::string{};
    if (op == "else_or") op = "or";
    auto right_start = next + 1;
    if (op == "is" && is("not", skip(right_start, multiline))) {
        op = "is not";
        right_start = skip(right_start, multiline) + 1;
    }
    const int rank = precedence(op);
    if (rank >= min && binding_power(node) >= rank) {
        const auto* left_binary = get<Binary>(node);
        if (!(rank == 30 && left_binary && precedence(left_binary->op) == 30)) {
            for (const auto& right : expressions(right_start, rank + 1, multiline)) {
                extend(make_node<Binary>(covering(right.pos), op, node, right.node), right.pos);
            }
        }
    }
    return results;
}
} // namespace

NodePtr parse(std::string_view source) { return Parser(tokenize(source)).program(); }
} // namespace slang
