#include "slang/runtime.hpp"
#include "slang/number.hpp"
#include "slang/parser.hpp"
#include "slang/graphics.hpp"
#include "slang/library.hpp"
#include "slang/window.hpp"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <thread>
#include <unordered_set>

namespace slang {
namespace {
struct UserMessage : std::runtime_error { using std::runtime_error::runtime_error; };
struct ListValue;
struct ObjectValue;
struct Callable;
struct Environment;
struct ModuleValue;
using Value = std::variant<std::monostate, Number, std::string, bool, ListValue*, ObjectValue*, Callable*, ModuleValue*>;
struct Type {
    std::string name;
    Callable* nominal = nullptr;
    std::shared_ptr<Type> element;
};
struct Cell { Value value; Type type; bool initialized = false; bool constant = false; };
struct Heap { bool marked = false; virtual ~Heap() = default; };
struct ListValue : Heap { std::vector<Value> items; std::optional<Type> element; };
struct ObjectValue : Heap { Callable* nominal = nullptr; bool window_control = false; std::vector<std::pair<std::string, Cell>> fields; };
struct Source { std::string text; std::string name; std::filesystem::path directory; };
struct SourceGuard {
    std::shared_ptr<Source>& current;
    std::shared_ptr<Source> previous;
    explicit SourceGuard(std::shared_ptr<Source>& source) : current(source), previous(source) {}
    ~SourceGuard() { current = previous; }
};
std::filesystem::path source_path(const std::string& path) {
    return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(path.data()), path.size()));
}
std::string path_text(const std::filesystem::path& path) {
    const auto text = path.u8string();
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}
struct Specialization { std::vector<Type> arguments; Type result; };
struct FunctionValue { NodePtr syntax; Environment* closure; std::vector<Type> parameters; std::vector<Specialization> returns; std::shared_ptr<Source> source; };
struct Callable : Heap {
    std::string name;
    std::string builtin;
    NodePtr object;
    Environment* closure = nullptr;
    std::shared_ptr<Source> source;
    std::vector<FunctionValue> overloads;
};
struct Environment : Heap { Environment* parent = nullptr; std::map<std::string, Cell> bindings; };
struct ModuleValue : Heap {
    std::string name;
    Environment* environment = nullptr;
    std::map<std::string, Cell*> exports;
};
struct ModuleRecord {
    std::filesystem::path path;
    ModuleValue* value = nullptr;
    bool loading = true;
    std::optional<Diagnostic> error;
};
struct Returned { Value value; };
struct LoopControl { bool stop; };
struct EvaluationDepth {
    std::size_t& depth;
    EvaluationDepth(std::size_t& active, const NodePtr& node) : depth(active) {
        if (depth >= 128) throw Diagnostic("R193", "Evaluation nesting limit reached", node->span);
        ++depth;
    }
    ~EvaluationDepth() { --depth; }
};
const std::vector<std::string> builtins = {"print", "input", "length", "append", "remove", "type", "round", "divide", "assert", "error", "list", "obj"};

Type value_type(const Value& value) {
    if (const auto* number = std::get_if<Number>(&value)) return {number->is_integer() ? "int" : "dec", nullptr, {}};
    if (std::holds_alternative<std::string>(value)) return {"text", nullptr, {}};
    if (std::holds_alternative<bool>(value)) return {"bool", nullptr, {}};
    if (std::holds_alternative<ListValue*>(value)) return {"list", nullptr, {}};
    if (const auto* object = std::get_if<ObjectValue*>(&value)) return {(*object)->nominal ? (*object)->nominal->name : "obj", (*object)->nominal, {}};
    if (std::holds_alternative<Callable*>(value)) return {"func", nullptr, {}};
    if (std::holds_alternative<ModuleValue*>(value)) return {"lib", nullptr, {}};
    return {};
}
bool same_type(const Type& a, const Type& b) {
    return a.name == b.name && a.nominal == b.nominal && bool(a.element) == bool(b.element) &&
        (!a.element || same_type(*a.element, *b.element));
}
bool truth(const Value& value) {
    if (std::holds_alternative<std::monostate>(value)) return false;
    if (const auto* n = std::get_if<Number>(&value)) return !n->zero();
    if (const auto* s = std::get_if<std::string>(&value)) return !s->empty();
    if (const auto* b = std::get_if<bool>(&value)) return *b;
    if (const auto* list = std::get_if<ListValue*>(&value)) return !(*list)->items.empty();
    return true;
}
bool equal(const Value& a, const Value& b) {
    if (a.index() != b.index()) return false;
    if (const auto* n = std::get_if<Number>(&a)) return Number::compare(*n, std::get<Number>(b)) == 0;
    if (const auto* s = std::get_if<std::string>(&a)) return *s == std::get<std::string>(b);
    if (const auto* v = std::get_if<bool>(&a)) return *v == std::get<bool>(b);
    if (const auto* v = std::get_if<ListValue*>(&a)) return *v == std::get<ListValue*>(b);
    if (const auto* v = std::get_if<ObjectValue*>(&a)) return *v == std::get<ObjectValue*>(b);
    if (const auto* v = std::get_if<Callable*>(&a)) return *v == std::get<Callable*>(b);
    if (const auto* v = std::get_if<ModuleValue*>(&a)) return *v == std::get<ModuleValue*>(b);
    return true;
}
std::string display(const Value& value, std::set<const Heap*>& active) {
    if (const auto* n = std::get_if<Number>(&value)) return n->str();
    if (const auto* s = std::get_if<std::string>(&value)) return *s;
    if (const auto* b = std::get_if<bool>(&value)) return *b ? "true" : "false";
    if (const auto* f = std::get_if<Callable*>(&value)) return "<func " + (*f)->name + ">";
    if (const auto* m = std::get_if<ModuleValue*>(&value)) return "<lib " + (*m)->name + ">";
    const Heap* address = nullptr;
    if (const auto* list = std::get_if<ListValue*>(&value)) address = *list;
    if (const auto* object = std::get_if<ObjectValue*>(&value)) address = *object;
    if (!address) return "none";
    if (active.size() >= 64) return "<...>";
    if (!active.insert(address).second) return "<cycle>";
    std::string out;
    if (const auto* list = std::get_if<ListValue*>(&value)) {
        out = "list<";
        for (const auto& item : (*list)->items) { if (out != "list<") out += ", "; out += display(item, active); }
    } else {
        const auto* object = std::get<ObjectValue*>(value);
        out = object->nominal ? object->nominal->name + "<" : "obj<";
        bool first = true;
        for (const auto& [name, cell] : object->fields) {
            if (!first) out += ", ";
            first = false;
            out += name + " = " + display(cell.value, active);
        }
    }
    active.erase(address);
    return out + ">";
}
std::string display(const Value& value) { std::set<const Heap*> active; return display(value, active); }
const Number& number(const Value& value) {
    const auto* result = std::get_if<Number>(&value);
    if (!result) throw std::runtime_error("Expected a number, found " + (value_type(value).name.empty() ? "none" : value_type(value).name));
    return *result;
}

// Preflight name and control-flow validation. All declarations belong to their
// lexical block; variables remain unreadable until their initializer executes.
struct Scope { Scope* parent; std::map<std::string, std::string> names; std::set<std::string> signatures; };
std::string signature_of(const Function& function) {
    std::string result = function.name + ":";
    for (const auto& param : function.parameters) result += (param.annotation ? ast_json(param.annotation) : "_") + ";";
    return result;
}
std::string lookup(Scope* scope, const std::string& name) {
    for (; scope; scope = scope->parent) if (auto it = scope->names.find(name); it != scope->names.end()) return it->second;
    return {};
}
[[noreturn]] void fail(const NodePtr& node, std::string message, std::string code = "R100") {
    throw Diagnostic(std::move(code), std::move(message), node->span);
}
void validate_node(const NodePtr&, Scope*, int, bool);
NodePtr declaration(const NodePtr& node) { const auto* p = get<Public>(node); return p ? p->declaration : node; }
void declare_names(const Nodes& nodes, Scope& scope) {
    for (const auto& entry : nodes) {
        const auto node = declaration(entry);
        std::string name, kind;
        if (const auto* v = get<Variable>(node)) { name = v->name; kind = "var"; }
        if (const auto* f = get<Function>(node); f && f->named) { name = f->name; kind = "func"; }
        if (const auto* o = get<Object>(node)) { name = o->name; kind = "obj"; }
        if (const auto* m = get<Library>(node)) { name = m->name; kind = "lib"; }
        if (const auto* i = get<Import>(node)) { name = i->alias; kind = "lib"; }
        if (name.empty()) continue;
        auto [it, inserted] = scope.names.emplace(name, kind);
        if (!inserted && !(kind == "func" && it->second == "func")) fail(node, "'" + name + "' is already declared in this scope", "R101");
        if (kind == "func" && !scope.signatures.insert(signature_of(*get<Function>(node))).second) fail(node, "Duplicate overload for '" + name + "'");
    }
}
void validate_body(const Nodes& nodes, Scope* parent, int loops, bool function, std::map<std::string, std::string> bindings = {}) {
    Scope local{parent, std::move(bindings), {}};
    declare_names(nodes, local);
    for (const auto& node : nodes) validate_node(node, &local, loops, function);
}
void validate_node(const NodePtr& node, Scope* scope, int loops, bool function) {
    if (!node) return;
    auto visit = [&](const NodePtr& child) { validate_node(child, scope, loops, function); };
    if (const auto* library = get<Library>(node)) {
        Scope isolated{nullptr, {}, {}};
        for (const auto& name : builtins) isolated.names[name] = "builtin";
        for (const auto& [alias, original] : builtins_aliases()) { (void)original; isolated.names[alias] = "builtin"; }
        std::map<std::string, bool> visibility;
        for (const auto& entry : library->body) if (const auto* fn = get<Function>(declaration(entry))) {
            const bool exported = get<Public>(entry) != nullptr;
            auto [it, inserted] = visibility.emplace(fn->name, exported);
            if (!inserted && it->second != exported) fail(entry, "All overloads of '" + fn->name + "' must have the same public/private visibility", "M101");
        }
        validate_body(library->body, &isolated, 0, false);
    }
    else if (const auto* exported = get<Public>(node)) visit(exported->declaration);
    else if (get<Import>(node)) {}
    else if (const auto* n = get<Name>(node)) { if (lookup(scope, n->name).empty()) fail(node, "Unknown name '" + n->name + "'; declare it with var or func first", "R102"); }
    else if (const auto* t = get<TypeName>(node)) {
        const auto dot = t->name.find('.');
        const bool qualified = dot != std::string::npos && lookup(scope, t->name.substr(0, dot)) == "lib";
        if (t->name != "int" && t->name != "dec" && t->name != "text" && t->name != "bool" && t->name != "list" && t->name != "obj" && lookup(scope, t->name) != "obj" && !qualified) fail(node, "Unknown type '" + t->name + "'", "R103");
        visit(t->element);
    }
    else if (const auto* v = get<Variable>(node)) { visit(v->annotation); visit(v->value); }
    else if (const auto* f = get<Function>(node)) {
        std::map<std::string, std::string> params;
        for (const auto& p : f->parameters) { visit(p.annotation); if (!params.emplace(p.name, "var").second) fail(node, "Duplicate parameter '" + p.name + "'"); }
        validate_body(f->body, scope, 0, true, std::move(params));
    } else if (const auto* o = get<Object>(node)) {
        std::set<std::string> names;
        for (const auto& field : o->fields) { visit(field.annotation); if (!names.insert(field.name).second) fail(node, "Duplicate field '" + field.name + "'"); }
    } else if (const auto* c = get<Conditional>(node)) {
        for (const auto& branch : c->branches) { visit(branch.condition); validate_body(branch.body, scope, loops, function); }
        if (c->otherwise) validate_body(*c->otherwise, scope, loops, function);
    } else if (const auto* w = get<While>(node)) { visit(w->condition); validate_body(w->body, scope, loops + 1, function); }
    else if (const auto* r = get<Repeat>(node)) { visit(r->count); validate_body(r->body, scope, loops + 1, function); }
    else if (const auto* each = get<ForEach>(node)) { visit(each->values); validate_body(each->body, scope, loops + 1, function, {{each->variable, "var"}}); }
    else if (const auto* range = get<ForRange>(node)) { visit(range->from); visit(range->to); visit(range->step); validate_body(range->body, scope, loops + 1, function, {{range->variable, "var"}}); }
    else if (const auto* returned = get<Return>(node)) { if (!function) fail(node, "ret/retur must be inside a function"); visit(returned->value); }
    else if (get<Break>(node) || get<Continue>(node)) { if (!loops) fail(node, "Loop control must be inside a loop in this function"); }
    else if (const auto* a = get<Assignment>(node)) { visit(a->target); visit(a->value); }
    else if (const auto* e = get<ExpressionStatement>(node)) visit(e->expression);
    else if (const auto* l = get<ListLiteral>(node)) { for (const auto& item : l->items) visit(item); }
    else if (const auto* call = get<Call>(node)) { visit(call->callee); for (const auto& arg : call->arguments) visit(arg.value); }
    else if (const auto* m = get<Member>(node)) visit(m->object);
    else if (const auto* i = get<Index>(node)) { visit(i->object); visit(i->index); }
    else if (const auto* conversion = get<Conversion>(node)) visit(conversion->value);
    else if (const auto* g = get<Group>(node)) visit(g->value);
    else if (const auto* u = get<Unary>(node)) visit(u->value);
    else if (const auto* update = get<Update>(node)) visit(update->value);
    else if (const auto* b = get<Binary>(node)) { visit(b->left); visit(b->right); }
}
} // namespace

struct Runtime::Impl {
    std::istream& input;
    std::ostream& output;
    std::function<bool()> interrupted;
    Graphics graphics;
    Window window{graphics};
    struct WindowBinding { ObjectValue* object; Value callback; };
    std::vector<WindowBinding> window_bindings;
    bool window_running = false;
    RuntimeHost host;
    bool callback_running = false;
    std::size_t event_ticks = 0, next_collection = 1024;
    std::vector<Heap*> active_heaps;
    std::vector<const Value*> active_values;
    std::vector<const std::vector<Value>*> active_vectors;
    struct Roots {
        Impl& owner;
        std::size_t heaps, values, vectors;
        explicit Roots(Impl& runtime) : owner(runtime), heaps(runtime.active_heaps.size()), values(runtime.active_values.size()), vectors(runtime.active_vectors.size()) {}
        ~Roots() { owner.active_heaps.resize(heaps); owner.active_values.resize(values); owner.active_vectors.resize(vectors); }
        void add(Heap* item) { owner.active_heaps.push_back(item); }
        void add(const Value& item) { owner.active_values.push_back(&item); }
        void add(const std::vector<Value>& items) { owner.active_vectors.push_back(&items); }
    };
    std::size_t max_steps, steps = 0, calls = 0, evaluation_depth = 0;
    std::vector<std::unique_ptr<Heap>> heap;
    std::vector<Heap*> pending_marks;
    std::shared_ptr<Source> source;
    std::vector<std::unique_ptr<ModuleRecord>> modules;
    std::map<std::string, ModuleValue*> native_modules;
    std::optional<std::mt19937_64> random_engine;
    std::chrono::steady_clock::time_point clock_start;
    Environment* prelude = nullptr;
    Environment* global = nullptr;
    Impl(std::istream& in, std::ostream& out, std::function<bool()> stop, std::size_t limit)
        : input(in), output(out), interrupted(std::move(stop)), graphics(interrupted), max_steps(limit) { reset(); }
    template<class T> T* allocate() { auto p = std::make_unique<T>(); auto* raw = p.get(); heap.push_back(std::move(p)); return raw; }
    Environment* environment(Environment* parent) { auto* env = allocate<Environment>(); env->parent = parent; return env; }
    void reset() {
        graphics.reset(); callback_running = false; next_collection = 1024;
        window.reset(); window_bindings.clear(); window_running = false;
        random_engine.reset(); clock_start = std::chrono::steady_clock::now();
        modules.clear(); native_modules.clear(); heap.clear(); pending_marks.clear(); source.reset();
        prelude = environment(nullptr); global = environment(prelude);
        for (const auto& name : builtins) { auto* f = allocate<Callable>(); f->name = name; f->builtin = name; prelude->bindings[name] = {f, {"func", nullptr, {}}, true, true}; }
        for (const auto& [alias, original] : builtins_aliases()) prelude->bindings[alias] = prelude->bindings.at(original);
    }
    void tick(const NodePtr& node) {
        if (host.poll && event_ticks % 256 == 0) host.poll();
        if (interrupted && interrupted()) fail(node, "Execution interrupted", "R190");
        if (++event_ticks % 1024 == 0 && graphics.active()) {
            graphics.service();
            if (graphics.user_closed()) fail(node, "Execution stopped because the canvas was closed", "R194");
        }
        if (max_steps && ++steps > max_steps) fail(node, "Execution limit reached; use --max-steps to change the limit", "R191");
    }
    Cell& binding(Environment* env, const std::string& name) {
        for (; env; env = env->parent) if (auto it = env->bindings.find(name); it != env->bindings.end()) return it->second;
        throw std::runtime_error("Unknown name '" + name + "'");
    }
    void checkpoint(const NodePtr& node, Environment* env) {
        if (!host.checkpoint) return;
        host.checkpoint({source ? source->name : "", node->span.begin.line, node->span.begin.column, calls}, [&] {
            std::vector<DebugVariable> values; std::set<std::string> seen;
            for (auto* scope = env; scope && scope != prelude; scope = scope->parent) {
                for (const auto& [name, cell] : scope->bindings) {
                    if (!seen.insert(name).second) continue;
                    if (values.size() == 100) return values;
                    std::string rendered;
                    if (!cell.initialized) rendered = "<not initialized>";
                    else if (const auto* list = std::get_if<ListValue*>(&cell.value)) rendered = "list<" + std::to_string((*list)->items.size()) + " items>";
                    else if (const auto* object = std::get_if<ObjectValue*>(&cell.value)) {
                        rendered = ((*object)->nominal ? (*object)->nominal->name : "obj") + std::string("<");
                        for (const auto& [field, item] : (*object)->fields) {
                            if (rendered.size() > 256) { rendered += "..."; break; }
                            const auto type = value_type(item.value).name;
                            const bool simple = type == "int" || type == "dec" || type == "bool" || type == "text" || type.empty();
                            rendered += field + " = " + (simple ? display(item.value).substr(0, 80) : "<" + type + ">") + ", ";
                        }
                        rendered += ">";
                    } else rendered = display(cell.value);
                    if (rendered.size() > 512) rendered = rendered.substr(0, 512) + "...";
                    values.push_back({name, cell.type.name.empty() ? "none" : cell.type.name, std::move(rendered)});
                }
            }
            return values;
        });
    }
    Value read(Cell& cell) {
        if (!cell.initialized) throw std::runtime_error("This variable has not been initialized yet");
        return cell.value;
    }
    Cell& exported(ModuleValue* module, const std::string& name) {
        const auto found = module->exports.find(name);
        if (found == module->exports.end()) throw std::runtime_error("Library '" + module->name + "' does not export '" + name + "' (private or unknown member)");
        return *found->second;
    }
    Value qualified_name(const std::string& name, Environment* env) {
        auto end = name.find('.');
        auto value = read(binding(env, name.substr(0, end)));
        while (end != std::string::npos) {
            const auto start = end + 1; end = name.find('.', start);
            const auto* module = std::get_if<ModuleValue*>(&value);
            if (!module) throw std::runtime_error("A qualified type name must refer to a library export");
            value = read(exported(*module, name.substr(start, end == std::string::npos ? end : end - start)));
        }
        return value;
    }
    Type annotation(const NodePtr& node, Environment* env) {
        if (!node) return {};
        try {
        const auto* t = get<TypeName>(node);
        Type result{t->name, nullptr, {}};
        if (t->element) result.element = std::make_shared<Type>(annotation(t->element, env));
        if (result.name != "int" && result.name != "dec" && result.name != "text" && result.name != "bool" && result.name != "list" && result.name != "obj") {
            const auto v = qualified_name(result.name, env);
            const auto* f = std::get_if<Callable*>(&v);
            if (!f || !(*f)->object) throw std::runtime_error("'" + result.name + "' is not an object type");
            result.nominal = *f;
            result.name = (*f)->name;
        }
        return result;
        } catch (const Diagnostic&) { throw; }
        catch (const std::exception& error) { fail(node, error.what(), "R103"); }
    }
    bool accepts(const Type& type, const Value& value) {
        if (type.name.empty() || std::holds_alternative<std::monostate>(value)) return true;
        if (type.name == "dec" && std::holds_alternative<Number>(value)) return true;
        if (type.name == "obj" && std::holds_alternative<ObjectValue*>(value)) return true;
        auto actual = value_type(value);
        if (type.name != actual.name || type.nominal != actual.nominal) return false;
        if (type.element) {
            auto* list = std::get<ListValue*>(value);
            if (list->element && !same_type(*type.element, *list->element)) return false;
            for (const auto& item : list->items) if (!accepts(*type.element, item)) return false;
        }
        return true;
    }
    Value coerce(const Type& type, Value value) {
        if (!accepts(type, value)) throw std::runtime_error("Expected " + type.name + ", found " + value_type(value).name + "; use an explicit conversion");
        if (std::holds_alternative<std::monostate>(value)) return value;
        if (type.name == "dec") value = number(value).as_decimal();
        if (type.element) {
            auto* list = std::get<ListValue*>(value);
            list->element = *type.element;
            for (auto& item : list->items) item = coerce(*type.element, item);
        }
        return value;
    }
    void assign(Cell& cell, Value value, bool initialize = false) {
        if (cell.constant && !initialize) throw std::runtime_error("Cannot assign to a declaration or imported namespace");
        if (!cell.initialized && !initialize) throw std::runtime_error("Cannot assign before this variable's declaration");
        Type target = cell.type.name.empty() ? value_type(value) : cell.type;
        value = coerce(target, std::move(value));
        cell.type = target; cell.value = std::move(value); cell.initialized = true;
    }
    ModuleValue* load(const Import& import, const NodePtr& site);
    ModuleValue* initialize_library(const NodePtr& node);
    void prepare(const Nodes& nodes, Environment* env) {
        Roots roots(*this); roots.add(env);
        // Resolve all imports before installing local declarations. Failed loads
        // leave the importing scope available for a corrected submission.
        std::vector<std::pair<std::string, ModuleValue*>> namespaces;
        for (const auto& node : nodes) {
            if (const auto* imported = get<Import>(node)) { auto* module = load(*imported, node); roots.add(module); namespaces.emplace_back(imported->alias, module); }
            if (const auto* library = get<Library>(node)) { auto* module = initialize_library(node); roots.add(module); namespaces.emplace_back(library->name, module); }
        }
        for (const auto& [name, module] : namespaces) env->bindings[name] = {module, {"lib", nullptr, {}}, true, true};
        for (const auto& entry : nodes) {
            const auto node = declaration(entry);
            if (const auto* v = get<Variable>(node)) {
                if (env->bindings.contains(v->name)) fail(node, "'" + v->name + "' is already declared");
                env->bindings[v->name] = {};
            }
            if (const auto* f = get<Function>(node); f && f->named) {
                if (!env->bindings.contains(f->name)) { auto* callable = allocate<Callable>(); callable->name = f->name; env->bindings[f->name] = {callable, {"func", nullptr, {}}, true, true}; }
            }
            if (const auto* o = get<Object>(node)) {
                auto* callable = allocate<Callable>(); callable->name = o->name; callable->object = node; callable->closure = env; callable->source = source;
                env->bindings[o->name] = {callable, {"func", nullptr, {}}, true, true};
            }
        }
        for (const auto& entry : nodes) {
            const auto node = declaration(entry);
            if (const auto* v = get<Variable>(node)) env->bindings[v->name].type = annotation(v->annotation, env);
            if (const auto* object = get<Object>(node)) for (const auto& field : object->fields) (void)annotation(field.annotation, env);
            if (const auto* f = get<Function>(node); f && f->named) {
                auto* callable = std::get<Callable*>(env->bindings[f->name].value);
                FunctionValue fn{node, env, {}, {}, source};
                for (const auto& p : f->parameters) fn.parameters.push_back(annotation(p.annotation, env));
                for (const auto& old : callable->overloads) {
                    if (old.parameters.size() != fn.parameters.size()) continue;
                    bool same = true;
                    for (std::size_t i = 0; i < fn.parameters.size(); ++i) same = same && same_type(old.parameters[i], fn.parameters[i]);
                    if (same) fail(node, "Duplicate overload for '" + f->name + "'");
                }
                callable->overloads.push_back(std::move(fn));
            }
        }
    }
    struct Reference {
        Cell* cell = nullptr;
        ListValue* list = nullptr;
        std::size_t index = 0;
        bool readonly = false;
        Heap* owner = nullptr;
    };
    std::size_t index_value(const Value& value, std::size_t size) {
        const auto index = number(value).size_value(size);
        if (!index) throw std::runtime_error("Indices start at 1");
        return index - 1;
    }
    Reference reference(const NodePtr& node, Environment* env) {
        if (const auto* n = get<Name>(node)) return {&binding(env, n->name), nullptr, 0, false, env};
        const auto* m = get<Member>(node);
        const auto* i = get<Index>(node);
        Value target = eval(m ? m->object : i->object, env);
        Roots roots(*this); roots.add(target);
        Value index;
        if (i) index = eval(i->index, env);
        if (const auto* module = std::get_if<ModuleValue*>(&target)) {
            if (!m) throw std::runtime_error("Libraries expose named exports, not indexed fields");
            return {&exported(*module, m->name), nullptr, 0, true, *module};
        }
        if (auto* object = std::get_if<ObjectValue*>(&target)) {
            if (m) {
                auto requested = m->name;
                if ((*object)->window_control) if (const auto found = windowFields_aliases().find(requested); found != windowFields_aliases().end()) requested = found->second;
                for (auto& [name, cell] : (*object)->fields) if (name == requested) return {&cell, nullptr, 0, false, *object};
                throw std::runtime_error("Object has no field '" + m->name + "'");
            }
            return {&(*object)->fields.at(index_value(index, (*object)->fields.size())).second, nullptr, 0, false, *object};
        }
        if (i) if (auto* list = std::get_if<ListValue*>(&target)) return {nullptr, *list, index_value(index, (*list)->items.size()), false, *list};
        throw std::runtime_error("Field/index access needs an object or list");
    }
    Value read(Reference ref) { return ref.cell ? read(*ref.cell) : ref.list->items.at(ref.index); }
    void write(Reference ref, Value value) {
        if (ref.readonly) throw std::runtime_error("Library exports are read-only");
        if (ref.cell) assign(*ref.cell, std::move(value));
        else {
            // RHS calls may remove list entries after the target was evaluated.
            if (ref.index >= ref.list->items.size()) throw std::runtime_error("List index is no longer in range");
            if (ref.list->element) value = coerce(*ref.list->element, std::move(value));
            ref.list->items[ref.index] = std::move(value);
        }
    }
    Value eval(const NodePtr& node, Environment* env);
    Value invoke(Callable* callable, const std::vector<Value>& args, const std::vector<Argument>& syntax, const NodePtr& site);
    Value builtin(const std::string& name, const std::vector<Value>& args, const std::vector<Argument>& syntax);
    Value graphics_call(const std::string& name, const std::vector<Value>& args, const std::vector<Argument>& syntax, const NodePtr& site);
    Value library_call(const std::string& name, const std::vector<Value>& args, const std::vector<Argument>& syntax, const NodePtr& site);
    Value window_call(const std::string& name, const std::vector<Value>& args, const std::vector<Argument>& syntax, const NodePtr& site);
    void window_sync(bool to_window);
    Number random_integer(const Number& low, const Number& high, const NodePtr& site);
    bool read_line(std::string& line) {
        if (host.input) { auto value = host.input(); if (!value) return false; line = std::move(*value); return true; }
        if (!graphics.active()) return static_cast<bool>(std::getline(input, line));
        // The blocking stream read stays off the SDL thread; event processing
        // remains on the runner's main thread while the student types.
        auto pending = std::async(std::launch::async, [&] { return static_cast<bool>(std::getline(input, line)); });
        while (pending.wait_for(std::chrono::milliseconds(8)) != std::future_status::ready) graphics.service();
        return pending.get();
    }
    void statement(const NodePtr& node, Environment* env, bool echo);
    void sequence(const Nodes& nodes, Environment* env, bool echo = false) {
        Roots roots(*this); roots.add(env);
        prepare(nodes, env);
        for (const auto& node : nodes) { statement(node, env, echo); maybe_collect(); }
    }
    bool iteration(const Nodes& nodes, Environment* parent, const std::string& name = {}, const Value& value = {}) {
        auto* local = environment(parent);
        if (!name.empty()) local->bindings[name] = {value, value_type(value), true, false};
        try { sequence(nodes, local); } catch (const LoopControl& control) { maybe_collect(); return !control.stop; }
        maybe_collect();
        return true;
    }
    void mark_value(const Value& value) {
        if (const auto* p = std::get_if<ListValue*>(&value)) mark(*p);
        if (const auto* p = std::get_if<ObjectValue*>(&value)) mark(*p);
        if (const auto* p = std::get_if<Callable*>(&value)) mark(*p);
        if (const auto* p = std::get_if<ModuleValue*>(&value)) mark(*p);
    }
    void mark_type(const Type& type) { if (type.nominal) mark(type.nominal); if (type.element) mark_type(*type.element); }
    void mark(Heap* item) {
        if (!item || item->marked) return;
        item->marked = true;
        pending_marks.push_back(item);
    }
    void trace(Heap* item) {
        if (auto* module = dynamic_cast<ModuleValue*>(item)) mark(module->environment);
        if (auto* env = dynamic_cast<Environment*>(item)) { mark(env->parent); for (const auto& [name, cell] : env->bindings) { (void)name; mark_value(cell.value); mark_type(cell.type); } }
        if (auto* list = dynamic_cast<ListValue*>(item)) { for (const auto& v : list->items) mark_value(v); if (list->element) mark_type(*list->element); }
        if (auto* obj = dynamic_cast<ObjectValue*>(item)) { mark(obj->nominal); for (const auto& [name, cell] : obj->fields) { (void)name; mark_value(cell.value); mark_type(cell.type); } }
        if (auto* f = dynamic_cast<Callable*>(item)) {
            mark(f->closure);
            for (const auto& fn : f->overloads) {
                mark(fn.closure); for (const auto& t : fn.parameters) mark_type(t);
                for (const auto& specialization : fn.returns) { mark_type(specialization.result); for (const auto& t : specialization.arguments) mark_type(t); }
            }
        }
    }
    void collect() {
        mark(global);
        for (auto* item : active_heaps) mark(item);
        for (auto* value : active_values) mark_value(*value);
        for (auto* values : active_vectors) for (const auto& value : *values) mark_value(value);
        for (const auto& module : modules) mark(module->value);
        for (const auto& [name, module] : native_modules) { (void)name; mark(module); }
        for (const auto& binding : window_bindings) { mark(binding.object); mark_value(binding.callback); }
        while (!pending_marks.empty()) { auto* item = pending_marks.back(); pending_marks.pop_back(); trace(item); }
        std::erase_if(heap, [](const auto& item) { return !item->marked; });
        for (auto& item : heap) item->marked = false;
        next_collection = heap.size() + 1024;
    }
    void maybe_collect() { if (heap.size() >= next_collection) collect(); }
    void execute(const NodePtr& program, bool echo) {
        Scope scope{nullptr, {}, {}};
        for (const auto& name : builtins) scope.names[name] = "builtin";
        for (const auto& [alias, original] : builtins_aliases()) { (void)original; scope.names[alias] = "builtin"; }
        for (const auto& [name, cell] : global->bindings) {
            const auto* f = std::get_if<Callable*>(&cell.value);
            scope.names[name] = std::holds_alternative<ModuleValue*>(cell.value) ? "lib" :
                (f && cell.constant ? ((*f)->object ? "obj" : (!(*f)->builtin.empty() ? "builtin" : "func")) : "var");
            if (f && cell.constant) for (const auto& overload : (*f)->overloads) scope.signatures.insert(signature_of(*get<Function>(overload.syntax)));
        }
        const auto& nodes = get<Program>(program)->body;
        if (std::count_if(nodes.begin(), nodes.end(), [](const auto& n) { return get<Library>(n) != nullptr; }) > 1) fail(program, "Only one lib declaration is allowed per file or console submission", "M102");
        declare_names(nodes, scope);
        for (const auto& node : nodes) validate_node(node, &scope, 0, false);
        steps = 0; calls = 0;
        try { sequence(nodes, global, echo); }
        catch (...) { if (graphics.active()) graphics.close(); collect(); throw; }
        collect();
        graphics.refresh();
    }
};

ModuleValue* Runtime::Impl::initialize_library(const NodePtr& node) {
    const auto* library = get<Library>(node);
    auto* module = allocate<ModuleValue>();
    Roots roots(*this); roots.add(module);
    module->name = library->name;
    module->environment = environment(prelude);
    // Recheck visibility/names for directly submitted libraries as well as imports.
    validate_node(node, nullptr, 0, false);
    sequence(library->body, module->environment);
    for (const auto& entry : library->body) if (const auto* p = get<Public>(entry)) {
        const auto* fn = get<Function>(p->declaration);
        const auto name = fn ? fn->name : get<Object>(p->declaration)->name;
        module->exports[name] = &module->environment->bindings.at(name);
    }
    return module;
}

ModuleValue* Runtime::Impl::load(const Import& import, const NodePtr& site) {
    tick(site);
    if (import.bundled) {
        const auto alias = modules_aliases().find(import.path);
        const auto canonical = alias == modules_aliases().end() ? import.path : alias->second;
        if (auto it = native_modules.find(canonical); it != native_modules.end()) return it->second;
        const auto found = library_exports().find(canonical);
        if (found == library_exports().end()) fail(site, "Bundled library '" + import.path + "' is not available", "M103");
        auto* module = allocate<ModuleValue>(); module->name = canonical; module->environment = environment(prelude);
        for (const auto& name : found->second) {
            if (canonical == "console" || (canonical == "numbers" && (name == "round" || name == "divide"))) {
                module->exports[name] = &prelude->bindings.at(name); continue;
            }
            auto* f = allocate<Callable>(); f->name = canonical + "." + name; f->builtin = f->name;
            auto& cell = module->environment->bindings[name]; cell = {f, {"func", nullptr, {}}, true, true}; module->exports[name] = &cell;
        }
        for (const auto& [alias_name, original] : function_aliases().at(canonical)) module->exports[alias_name] = module->exports.at(original);
        native_modules[canonical] = module;
        return module;
    }
    if (import.path.empty() || import.path.find('\0') != std::string::npos) fail(site, "Import path must be non-empty text without NUL characters", "M104");
    const auto directory = source ? source->directory : std::filesystem::current_path();
    std::error_code error;
    auto path = std::filesystem::canonical(directory / source_path(import.path), error);
    if (error || !std::filesystem::is_regular_file(path)) fail(site, "Cannot open library '" + import.path + "' relative to " + path_text(directory), "M105");
    for (const auto& existing : modules) {
        std::error_code equivalent_error;
        if (existing->path != path && !std::filesystem::equivalent(existing->path, path, equivalent_error)) continue;
        if (existing->loading) {
            std::string chain;
            for (const auto& entry : modules) if (entry->loading) chain += path_text(entry->path.filename()) + " -> ";
            fail(site, "Circular import: " + chain + path_text(path.filename()), "M106");
        }
        if (existing->error) throw *existing->error;
        return existing->value;
    }
    if (std::count_if(modules.begin(), modules.end(), [](const auto& m) { return m->loading; }) >= 32) fail(site, "Library import depth limit reached (32 modules)", "M107");
    auto record = std::make_unique<ModuleRecord>(); record->path = path;
    auto* cached = record.get(); modules.push_back(std::move(record));
    SourceGuard restore(source);
    source = std::make_shared<Source>(Source{{}, path_text(path), path.parent_path()});
    try {
        std::ifstream file(path, std::ios::binary);
        if (!file) fail(site, "Cannot open library '" + path_text(path) + "'", "M105");
        source->text = {std::istreambuf_iterator<char>(file), {}};
        if (file.bad()) fail(site, "Cannot read library '" + path_text(path) + "'", "M105");
        const auto ast = parse(source->text);
        const auto& body = get<Program>(ast)->body;
        if (body.size() != 1 || !get<Library>(body[0])) fail(ast, "An imported file must contain exactly one lib block and no declarations or statements outside it", "M108");
        cached->value = initialize_library(body[0]);
        cached->loading = false;
        return cached->value;
    } catch (Diagnostic& diagnostic) {
        if (diagnostic.source_name.empty()) { diagnostic.source_name = source->name; diagnostic.source_text = source->text; }
        cached->error = diagnostic; cached->loading = false;
        throw;
    } catch (const std::exception& exception) {
        Diagnostic diagnostic("M109", exception.what(), site->span);
        diagnostic.source_name = source->name; diagnostic.source_text = source->text;
        cached->error = diagnostic; cached->loading = false;
        throw diagnostic;
    }
}

Value Runtime::Impl::eval(const NodePtr& node, Environment* env) {
    if (!node) return {};
    Roots roots(*this); roots.add(env);
    EvaluationDepth depth(evaluation_depth, node);
    tick(node);
    try {
        if (const auto* l = get<Literal>(node)) {
            switch (l->kind) {
            case LiteralKind::Integer: return Number::parse(l->value);
            case LiteralKind::Decimal: return Number::parse(l->value, true);
            case LiteralKind::Text: return l->value;
            case LiteralKind::Boolean: return l->value == "true";
            case LiteralKind::None: return {};
            }
        }
        if (const auto* n = get<Name>(node)) return read(binding(env, n->name));
        if (const auto* g = get<Group>(node)) return eval(g->value, env);
        if (const auto* l = get<ListLiteral>(node)) {
            auto* list = allocate<ListValue>();
            roots.add(list);
            for (const auto& item : l->items) list->items.push_back(eval(item, env));
            return list;
        }
        if (get<Member>(node) || get<Index>(node)) return read(reference(node, env));
        if (const auto* f = get<Function>(node)) {
            auto* callable = allocate<Callable>(); callable->name = "anonymous";
            FunctionValue fn{node, env, {}, {}, source};
            for (const auto& p : f->parameters) fn.parameters.push_back(annotation(p.annotation, env));
            callable->overloads.push_back(std::move(fn));
            return callable;
        }
        if (const auto* c = get<Call>(node)) {
            const auto callee = eval(c->callee, env);
            roots.add(callee);
            const auto* callable = std::get_if<Callable*>(&callee);
            if (!callable) throw std::runtime_error("Only functions and object constructors can be called");
            std::vector<Value> args;
            roots.add(args);
            for (const auto& arg : c->arguments) args.push_back(eval(arg.value, env));
            return invoke(*callable, args, c->arguments, node);
        }
        if (const auto* c = get<Conversion>(node)) {
            const auto value = eval(c->value, env);
            if (c->target == "text") return display(value);
            if (c->target == "bool") return truth(value);
            if (c->target == "int" || c->target == "dec") {
                const bool decimal = c->target == "dec";
                if (const auto* s = std::get_if<std::string>(&value)) return Number::parse(*s, decimal);
                if (const auto* b = std::get_if<bool>(&value)) return decimal ? Number::integer(*b ? 1 : 0).as_decimal() : Number::integer(*b ? 1 : 0);
                return decimal ? number(value).as_decimal() : number(value).as_integer();
            }
            return coerce({c->target, nullptr, {}}, value);
        }
        if (const auto* u = get<Unary>(node)) {
            auto value = eval(u->value, env);
            if (u->op == "not") return !truth(value);
            return u->op == "-" ? number(value).negate() : number(value);
        }
        if (const auto* u = get<Update>(node)) {
            auto target = reference(u->value, env);
            const auto old = read(target);
            const auto next = Number::add(number(old), Number::parse(u->op == "++" ? "1" : "-1"));
            write(target, next);
            return u->prefix ? Value(next) : old;
        }
        if (const auto* b = get<Binary>(node)) {
            auto left = eval(b->left, env);
            roots.add(left);
            if (b->op == "and") return truth(left) && truth(eval(b->right, env));
            if (b->op == "or") return truth(left) || truth(eval(b->right, env));
            auto right = eval(b->right, env);
            if (b->op == "is") return equal(left, right);
            if (b->op == "is not") return !equal(left, right);
            if (b->op == "+" && std::holds_alternative<std::string>(left) && std::holds_alternative<std::string>(right)) return std::get<std::string>(left) + std::get<std::string>(right);
            const auto& a = number(left); const auto& c = number(right);
            if (b->op == "+") return Number::add(a, c);
            if (b->op == "-") return Number::subtract(a, c);
            if (b->op == "*") return Number::multiply(a, c);
            if (b->op == "/") return Number::divide(a, c);
            if (b->op == "%") return Number::remainder(a, c);
            const int order = Number::compare(a, c);
            if (b->op == "<") return order < 0;
            if (b->op == ">") return order > 0;
            if (b->op == "<=") return order <= 0;
            if (b->op == ">=") return order >= 0;
        }
        throw std::runtime_error("Unsupported expression");
    } catch (const Diagnostic&) { throw; }
    catch (const UserMessage& error) { Diagnostic diagnostic("R100", error.what(), node->span); diagnostic.user_message = true; throw diagnostic; }
    catch (const std::exception& error) { fail(node, error.what()); }
}

Value Runtime::Impl::builtin(const std::string& name, const std::vector<Value>& args, const std::vector<Argument>& syntax) {
    const auto count = [&](std::size_t min, std::size_t max) { if (args.size() < min || args.size() > max) throw std::runtime_error(name + " received the wrong number of arguments"); };
    if (name != "obj") for (const auto& arg : syntax) if (arg.name) throw std::runtime_error("Named arguments are only supported by obj constructors");
    if (name == "print") {
        for (std::size_t i = 0; i < args.size(); ++i) { if (i) output << ' '; output << display(args[i]); }
        output << '\n'; return {};
    }
    if (name == "input") {
        count(0, 1); if (!args.empty()) output << display(args[0]); output.flush();
        std::string line; if (!read_line(line)) return {};
        if (!line.empty() && line.back() == '\r') line.pop_back();
        return line;
    }
    if (name == "type") { count(1, 1); auto type = value_type(args[0]); return type.name.empty() ? std::string("none") : type.name; }
    if (name == "list") { auto* list = allocate<ListValue>(); list->items = args; return list; }
    if (name == "obj") {
        auto* object = allocate<ObjectValue>(); std::set<std::string> names;
        for (std::size_t i = 0; i < args.size(); ++i) {
            const auto field = syntax[i].name.value_or(std::to_string(i + 1));
            if (!names.insert(field).second) throw std::runtime_error("Duplicate object field '" + field + "'");
            object->fields.push_back({field, {args[i], value_type(args[i]), true, false}});
        }
        return object;
    }
    if (name == "length") {
        count(1, 1);
        if (const auto* s = std::get_if<std::string>(&args[0])) return Number::integer(static_cast<std::size_t>(std::count_if(s->begin(), s->end(), [](unsigned char c) { return (c & 0xc0) != 0x80; })));
        if (const auto* l = std::get_if<ListValue*>(&args[0])) return Number::integer((*l)->items.size());
        if (const auto* o = std::get_if<ObjectValue*>(&args[0])) return Number::integer((*o)->fields.size());
        throw std::runtime_error("length needs text, a list, or an object");
    }
    if (name == "append" || name == "remove") {
        count(2, 2);
        const auto* l = std::get_if<ListValue*>(&args[0]);
        if (!l) throw std::runtime_error(name + " needs a list as its first argument");
        auto* list = *l;
        if (name == "append") { auto value = args[1]; if (list->element) value = coerce(*list->element, value); list->items.push_back(value); return {}; }
        const auto index = index_value(args[1], list->items.size());
        auto value = list->items[index]; list->items.erase(list->items.begin() + static_cast<std::ptrdiff_t>(index)); return value;
    }
    if (name == "round") { count(2, 2); return number(args[0]).rounded(number(args[1]).size_value(10000)); }
    if (name == "divide") { count(3, 3); return Number::divide(number(args[0]), number(args[1]), number(args[2]).size_value(10000)); }
    if (name == "assert") {
        count(1, 2); if (!truth(args[0])) throw std::runtime_error(args.size() == 2 ? "Assertion failed: " + display(args[1]) : "Assertion failed");
        return {};
    }
    if (name == "error") {
        count(1, 1);
        const auto* message = std::get_if<std::string>(&args[0]);
        if (!message) throw std::runtime_error("error needs a text message");
        throw UserMessage(*message);
    }
    throw std::runtime_error("Unknown builtin '" + name + "'");
}

Number Runtime::Impl::random_integer(const Number& low, const Number& high, const NodePtr& site) {
    if (!low.is_integer() || !high.is_integer()) throw std::runtime_error("random.integer needs int bounds");
    if (Number::compare(low, high) > 0) throw std::runtime_error("random.integer needs minimum <= maximum");
    const auto maximum = Number::subtract(high, low).str();
    if (maximum.size() > 10000) throw std::runtime_error("Random range exceeds 10000 digits");
    if (maximum == "0") return low;
    if (!random_engine) {
        std::random_device entropy;
        std::seed_seq seed{entropy(), entropy(), entropy(), entropy()};
        random_engine.emplace(seed);
    }
    // Rejection sampling over decimal digits supports arbitrary int bounds and
    // has the same seeded sequence on every platform (no STL distributions).
    for (;;) {
        tick(site);
        std::string candidate;
        for (std::size_t i = 0; i < maximum.size(); ++i) {
            if (i % 256 == 0) tick(site);
            std::uint64_t digit;
            do { digit = (*random_engine)() & 15; } while (digit >= 10);
            candidate += static_cast<char>('0' + digit);
        }
        if (candidate <= maximum) return Number::add(low, Number::parse(candidate));
    }
}

Value Runtime::Impl::library_call(const std::string& qualified, const std::vector<Value>& args, const std::vector<Argument>& syntax, const NodePtr& site) {
    for (const auto& arg : syntax) if (arg.name) throw std::runtime_error(qualified + " takes positional arguments");
    const auto dot = qualified.find('.');
    const auto library = qualified.substr(0, dot), name = qualified.substr(dot + 1);
    auto count = [&](std::size_t min, std::size_t max) {
        if (args.size() < min || args.size() > max) throw std::runtime_error(qualified + " received the wrong number of arguments");
    };
    auto text = [&](std::size_t index) -> const std::string& {
        const auto* value = std::get_if<std::string>(&args.at(index));
        if (!value) throw std::runtime_error(qualified + " needs text for argument " + std::to_string(index + 1));
        return *value;
    };
    auto list = [&](std::size_t index) -> ListValue* {
        const auto* value = std::get_if<ListValue*>(&args.at(index));
        if (!value) throw std::runtime_error(qualified + " needs a list for argument " + std::to_string(index + 1));
        return *value;
    };
    auto append_text = [&](std::string& target, const std::string& value) {
        if (value.size() > library_text_limit || target.size() > library_text_limit - value.size())
            throw std::runtime_error(qualified + " result exceeds 16 MiB of text");
        target += value;
    };
    auto slice = [&](std::size_t size) {
        const auto start = number(args[1]).size_value(size + 1);
        if (!start) throw std::runtime_error("Indices start at 1");
        const auto available = size - (start - 1);
        const auto amount = args.size() == 3 ? number(args[2]).size_value(available) : available;
        return std::pair{start - 1, amount};
    };
    auto progress = [&](std::size_t i) { if (i % 256 == 0) { if (host.poll) host.poll(); tick(site); } };
    if (library == "console" || (library == "numbers" && (name == "round" || name == "divide"))) return builtin(name, args, syntax);
    if (library == "numbers") {
        if (name == "parse" || name == "format") {
            count(name == "parse" ? 1 : 2, name == "parse" ? 2 : 3);
            const auto separator_index = name == "parse" ? 1u : 2u;
            const auto separator = args.size() > separator_index ? text(separator_index) : ".";
            if (separator != "." && separator != ",") throw std::runtime_error("Decimal separator must be '.' or ','");
            if (name == "parse") {
                auto value = text_trim(text(0));
                if (separator == ",") {
                    if (value.find('.') != std::string::npos) throw std::runtime_error("Expected ',' as the decimal separator; grouping separators are not supported");
                    std::replace(value.begin(), value.end(), ',', '.');
                }
                return Number::parse(value, true);
            }
            const auto places = number(args[1]).size_value(10000);
            auto value = number(args[0]).rounded(places).str();
            if (places) {
                auto position = value.find('.');
                if (position == std::string::npos) { position = value.size(); value += '.'; }
                value.append(places - (value.size() - position - 1), '0');
                value[position] = separator[0];
            }
            return value;
        }
        if (name == "sum" || name == "mean") {
            count(1, 1); const auto* values = list(0);
            if (name == "mean" && values->items.empty()) throw std::runtime_error("numbers.mean needs a non-empty list");
            auto total = Number::integer(0);
            for (std::size_t i = 0; i < values->items.size(); ++i) { progress(i); total = Number::add(total, number(values->items[i])); }
            return name == "mean" ? Number::divide(total, Number::integer(values->items.size())) : total;
        }
        if (name == "abs" || name == "floor" || name == "ceil") {
            count(1, 1); const auto& value = number(args[0]);
            if (name == "floor") return value.floored();
            if (name == "ceil") return value.ceiled();
            return value.negative() ? value.negate() : value;
        }
        if (name == "sqrt") {
            count(1, 2); return number(args[0]).square_root(args.size() == 2 ? number(args[1]).size_value(10000) : 28);
        }
        if (name == "pow") {
            count(2, 2); auto base = number(args[0]); auto exponent = number(args[1]).size_value(10000);
            auto result = base.is_integer() ? Number::integer(1) : Number::integer(1).as_decimal();
            while (exponent) { progress(0); if (exponent % 2) result = Number::multiply(result, base); exponent /= 2; if (exponent) base = Number::multiply(base, base); }
            return result;
        }
        count(name == "clamp" ? 3 : 2, name == "clamp" ? 3 : 2);
        auto result = number(args[0]); const auto& second = number(args[1]);
        if (name == "min" && Number::compare(result, second) > 0) result = second;
        if (name == "max" && Number::compare(result, second) < 0) result = second;
        if (name == "clamp") {
            const auto& high = number(args[2]);
            if (Number::compare(second, high) > 0) throw std::runtime_error("numbers.clamp needs minimum <= maximum");
            if (Number::compare(result, second) < 0) result = second;
            if (Number::compare(result, high) > 0) result = high;
        }
        for (const auto& arg : args) if (!number(arg).is_integer()) result = result.as_decimal();
        return result;
    }
    if (library == "strings") {
        if (name == "join") {
            count(2, 2); const auto* values = list(0); const auto& separator = text(1); std::string out;
            for (std::size_t i = 0; i < values->items.size(); ++i) {
                progress(i);
                const auto* value = std::get_if<std::string>(&values->items[i]);
                if (!value) throw std::runtime_error("strings.join needs a list of text");
                if (i) append_text(out, separator);
                append_text(out, *value);
            }
            return out;
        }
        if (name == "length" || name == "trim" || name == "lower" || name == "upper") {
            count(1, 1); const auto& value = text(0);
            if (name == "length") return Number::integer(text_offsets(value).size() - 1);
            if (name == "trim") return text_trim(value);
            return text_case(value, name == "upper");
        }
        count(name == "replace" ? 3 : 2, name == "slice" || name == "replace" ? 3 : 2);
        const auto& value = text(0); const auto offsets = text_offsets(value);
        if (name == "slice") {
            const auto [start, amount] = slice(offsets.size() - 1);
            return value.substr(offsets[start], offsets[start + amount] - offsets[start]);
        }
        const auto& search = text(1); (void)text_offsets(search);
        if (name == "contains" || name == "find") {
            const auto position = value.find(search);
            if (name == "contains") return position != std::string::npos;
            if (position == std::string::npos) return {};
            return Number::integer(static_cast<std::size_t>(std::lower_bound(offsets.begin(), offsets.end(), position) - offsets.begin()) + 1);
        }
        if (search.empty()) throw std::runtime_error(qualified + " needs a non-empty separator or search text");
        if (name == "split") {
            auto* result = allocate<ListValue>(); result->element = Type{"text", nullptr, {}};
            std::size_t start = 0;
            for (;;) {
                progress(result->items.size());
                if (result->items.size() >= library_list_limit) throw std::runtime_error("strings.split result exceeds 1000000 items");
                const auto end = value.find(search, start);
                result->items.emplace_back(value.substr(start, end == std::string::npos ? end : end - start));
                if (end == std::string::npos) return result;
                start = end + search.size();
            }
        }
        const auto& replacement = text(2); (void)text_offsets(replacement);
        std::string out; std::size_t start = 0, iterations = 0;
        for (;;) {
            progress(iterations++);
            const auto end = value.find(search, start);
            append_text(out, value.substr(start, end == std::string::npos ? end : end - start));
            if (end == std::string::npos) return out;
            append_text(out, replacement); start = end + search.size();
        }
    }
    if (library == "lists") {
        const bool two = name == "append" || name == "remove" || name == "find" || name == "contains" || name == "slice";
        count(two ? 2 : 1, name == "slice" ? 3 : two ? 2 : 1);
        auto* values = list(0);
        if (name == "length" || name == "append" || name == "remove") return builtin(name, args, syntax);
        if (name == "contains" || name == "find") {
            for (std::size_t i = 0; i < values->items.size(); ++i) {
                progress(i);
                if (equal(values->items[i], args[1])) return name == "contains" ? Value{true} : Value{Number::integer(i + 1)};
            }
            return name == "contains" ? Value{false} : Value{};
        }
        auto* result = allocate<ListValue>(); result->element = values->element;
        if (name == "slice") {
            const auto [start, amount] = slice(values->items.size());
            result->items.assign(values->items.begin() + static_cast<std::ptrdiff_t>(start), values->items.begin() + static_cast<std::ptrdiff_t>(start + amount));
        } else result->items = values->items;
        if (name == "reverse") std::reverse(result->items.begin(), result->items.end());
        if (name == "sort" && !result->items.empty()) {
            const bool numeric = std::holds_alternative<Number>(result->items[0]);
            for (const auto& value : result->items) if (!(numeric ? std::holds_alternative<Number>(value) : std::holds_alternative<std::string>(value)))
                throw std::runtime_error("lists.sort needs all numbers or all text");
            std::size_t comparisons = 0;
            std::stable_sort(result->items.begin(), result->items.end(), [&](const Value& a, const Value& b) {
                progress(comparisons++);
                return numeric ? Number::compare(number(a), number(b)) < 0 : std::get<std::string>(a) < std::get<std::string>(b);
            });
        }
        return result;
    }
    if (library == "random") {
        if (name == "seed") {
            count(1, 1); const auto& value = number(args[0]);
            if (!value.is_integer()) throw std::runtime_error("random.seed needs an int");
            const auto digits = value.str(); std::seed_seq seed(digits.begin(), digits.end()); random_engine.emplace(seed); return {};
        }
        if (name == "integer") { count(2, 2); return random_integer(number(args[0]), number(args[1]), site); }
        count(1, 1); const auto* values = list(0);
        if (name == "choice") {
            if (values->items.empty()) throw std::runtime_error("random.choice needs a non-empty list");
            return values->items[random_integer(Number::integer(0), Number::integer(values->items.size() - 1), site).size_value(values->items.size() - 1)];
        }
        auto* result = allocate<ListValue>(); result->items = values->items; result->element = values->element;
        for (std::size_t i = result->items.size(); i > 1; --i) {
            const auto j = random_integer(Number::integer(0), Number::integer(i - 1), site).size_value(i - 1);
            std::swap(result->items[i - 1], result->items[j]);
        }
        return result;
    }
    if (library == "time") {
        if (name == "seconds") {
            count(0, 0);
            const auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - clock_start).count();
            return Number::divide(Number::parse(std::to_string(nanos)), Number::integer(1000000000), 9);
        }
        count(1, 1); const auto& seconds = number(args[0]);
        if (seconds.negative() || Number::compare(seconds, Number::integer(86400)) > 0) throw std::runtime_error("time.sleep needs seconds between 0 and 86400");
        const auto micros = Number::multiply(seconds, Number::integer(1000000)).rounded(0).as_integer().size_value(86400000000ULL);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::microseconds(micros);
        do {
            if (host.poll) host.poll();
            if (interrupted && interrupted()) fail(site, "Execution interrupted", "R190");
            if (graphics.active()) { graphics.service(); if (graphics.user_closed()) fail(site, "Execution stopped because the canvas was closed", "R194"); }
            const auto remaining = deadline - std::chrono::steady_clock::now();
            if (remaining <= std::chrono::steady_clock::duration::zero()) break;
            std::this_thread::sleep_for(std::min(remaining, std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::milliseconds(8))));
        } while (true);
        return {};
    }
    if (library == "files") {
        count(name == "write" || name == "append" ? 2 : 1, name == "write" || name == "append" ? 2 : 1);
        const auto& filename = text(0);
        if (filename.empty() || filename.find('\0') != std::string::npos) throw std::runtime_error("File path must be non-empty text without NUL characters");
        (void)text_offsets(filename);
        const auto path = (source ? source->directory : std::filesystem::current_path()) / source_path(filename);
        std::error_code error; const bool exists = std::filesystem::exists(path, error);
        if (error) throw std::runtime_error("Cannot access file '" + filename + "': " + error.message());
        if (name == "exists") return exists;
        if (exists && !std::filesystem::is_regular_file(path)) throw std::runtime_error("Expected a regular file: " + filename);
        if (name == "read") {
            std::ifstream stream(path, std::ios::binary);
            if (!stream) throw std::runtime_error("Cannot read file '" + filename + "'");
            std::string result; char chunk[8192];
            while (stream.read(chunk, sizeof(chunk)) || stream.gcount()) {
                progress(0); append_text(result, std::string(chunk, static_cast<std::size_t>(stream.gcount())));
            }
            if (!stream.eof()) throw std::runtime_error("Failed reading file '" + filename + "'");
            if (result.starts_with("\xef\xbb\xbf")) result.erase(0, 3);
            (void)text_offsets(result); return result;
        }
        const auto& content = text(1);
        if (content.size() > library_text_limit) throw std::runtime_error("File write exceeds 16 MiB of text");
        (void)text_offsets(content); // Validate before opening/truncating the destination.
        std::ofstream stream(path, std::ios::binary | (name == "append" ? std::ios::app : std::ios::trunc));
        if (!stream) throw std::runtime_error("Cannot write file '" + filename + "'");
        stream.write(content.data(), static_cast<std::streamsize>(content.size())); stream.close();
        if (!stream) throw std::runtime_error("Failed writing file '" + filename + "'");
        return {};
    }
    throw std::runtime_error("Unknown library function '" + qualified + "'");
}

void Runtime::Impl::window_sync(bool to_window) {
    auto string_value = [](const Value& value) -> std::string {
        const auto* text = std::get_if<std::string>(&value);
        if (!text || text->size() > 4096) throw std::runtime_error("Window text must be text of at most 4096 UTF-8 bytes");
        (void)text_offsets(*text); return *text;
    };
    for (std::size_t i = 0; i < window_bindings.size(); ++i) {
        auto* object = window_bindings[i].object; auto& widget = window.widgets()[i];
        auto field = [&](const std::string& name) -> Cell& {
            for (auto& [key, cell] : object->fields) if (key == name) return cell;
            throw std::runtime_error("Missing window property: " + name);
        };
        const bool choices = widget.kind == WidgetKind::checklist || widget.kind == WidgetKind::radio || widget.kind == WidgetKind::listbox || widget.kind == WidgetKind::combo;
        if (to_window) {
            widget.text = string_value(field("caption").value);
            const auto* enabled = std::get_if<bool>(&field("enabled").value);
            if (!enabled) throw std::runtime_error("Window enabled must be bool"); widget.enabled = *enabled;
            if (widget.kind == WidgetKind::input) widget.value = string_value(field("value").value);
            if (choices) {
                const auto* list = std::get_if<ListValue*>(&field("items").value);
                if (!list || (*list)->items.size() > 100) throw std::runtime_error("Window choices need a list of at most 100 text items");
                widget.items.clear(); std::set<std::string> unique;
                for (const auto& item : (*list)->items) {
                    auto label = string_value(item);
                    if (!unique.insert(label).second) throw std::runtime_error("Window choice items must be unique");
                    widget.items.push_back(std::move(label));
                }
                if (widget.kind == WidgetKind::checklist) {
                    const auto* selected = std::get_if<ListValue*>(&field("selected").value);
                    if (!selected) throw std::runtime_error("Checklist selected must be a list of text");
                    widget.selected.assign(widget.items.size(), false);
                    for (const auto& item : (*selected)->items) {
                        const auto label = string_value(item); const auto found = std::find(widget.items.begin(), widget.items.end(), label);
                        if (found == widget.items.end()) throw std::runtime_error("Checklist selected value is not in items");
                        widget.selected[static_cast<std::size_t>(found - widget.items.begin())] = true;
                    }
                } else {
                    const auto& value = field("value").value;
                    widget.value = std::holds_alternative<std::monostate>(value) ? "" : string_value(value);
                    widget.choice = -1;
                    if (!std::holds_alternative<std::monostate>(value)) {
                        const auto found = std::find(widget.items.begin(), widget.items.end(), widget.value);
                        if (found == widget.items.end()) throw std::runtime_error("Window value is not in items; choose an item or none");
                        widget.choice = static_cast<int>(found - widget.items.begin());
                    }
                }
            }
        } else {
            if (widget.kind == WidgetKind::input) assign(field("value"), widget.value);
            if (widget.kind == WidgetKind::checklist) {
                auto* list = std::get<ListValue*>(field("selected").value); list->items.clear();
                for (std::size_t j = 0; j < widget.items.size(); ++j) if (widget.selected[j]) list->items.emplace_back(widget.items[j]);
            } else if (choices) assign(field("value"), widget.choice < 0 ? Value{} : Value{widget.value});
        }
    }
}

Value Runtime::Impl::window_call(const std::string& name, const std::vector<Value>& args, const std::vector<Argument>& syntax, const NodePtr& site) {
    for (const auto& arg : syntax) if (arg.name) throw std::runtime_error("Window functions take positional arguments");
    auto count = [&](std::size_t min, std::size_t max) { if (args.size() < min || args.size() > max) throw std::runtime_error("window." + name + " received the wrong number of arguments"); };
    auto text = [&](std::size_t index) -> std::string {
        const auto* value = std::get_if<std::string>(&args.at(index));
        if (!value || value->size() > 4096) throw std::runtime_error("window." + name + " needs text of at most 4096 UTF-8 bytes");
        (void)text_offsets(*value); return *value;
    };
    if (name == "close") { count(0, 0); window.close(); return {}; }
    if (name == "create") {
        count(1, 3);
        if (window_running || callback_running) throw std::runtime_error("Create a window before starting its event loop");
        const auto title = text(0);
        const auto width = args.size() > 1 ? number(args[1]).size_value(1600) : 480;
        const auto height = args.size() > 2 ? number(args[2]).size_value(1600) : 640;
        window.create(title, static_cast<int>(width), static_cast<int>(height)); window_bindings.clear(); return {};
    }
    if (!window.active()) throw std::runtime_error("Call window.create before adding controls or running the window");
    if (name == "run") {
        count(0, 0);
        if (window_running || callback_running) throw std::runtime_error("Cannot nest window event loops");
        struct Running { bool& flag; explicit Running(bool& value) : flag(value) { flag = true; } ~Running() { flag = false; } } running(window_running);
        window_sync(true);
        while (window.active() && graphics.open()) {
            tick(site); window_sync(true);
            const auto activated = window.update(); window_sync(false);
            for (auto index : activated) {
                if (!window.active()) break;
                const auto callback = window_bindings[index].callback;
                (void)invoke(std::get<Callable*>(callback), {}, {}, site);
            }
            if (!window.active()) break;
            window_sync(true); window.draw(); graphics.present(); steps = 0; maybe_collect();
        }
        return {};
    }
    if (window_running) throw std::runtime_error("Add controls before window.run; callbacks can change their properties");
    if (window.widgets().size() >= 100) throw std::runtime_error("A window supports up to 100 controls");
    const bool choice = name == "checklist" || name == "radio_group" || name == "listbox" || name == "combobox";
    count(choice || name == "button" ? 2 : 1, choice || name == "button" || name == "input" ? 2 : 1);
    Widget widget{}; widget.text = text(0);
    widget.kind = name == "label" ? WidgetKind::label : name == "input" ? WidgetKind::input : name == "button" ? WidgetKind::button : name == "checklist" ? WidgetKind::checklist : name == "radio_group" ? WidgetKind::radio : name == "listbox" ? WidgetKind::listbox : WidgetKind::combo;
    Value callback;
    if (name == "button") {
        const auto* function = std::get_if<Callable*>(&args[1]);
        if (!function || (*function)->object) throw std::runtime_error("window.button needs a function taking no arguments"); callback = args[1];
    }
    if (name == "input") widget.value = args.size() == 2 ? text(1) : "";
    if (choice) {
        const auto* list = std::get_if<ListValue*>(&args[1]);
        if (!list || (*list)->items.size() > 100) throw std::runtime_error("Window choices need a list of at most 100 text items");
        std::set<std::string> unique;
        for (const auto& item : (*list)->items) {
            const auto* label = std::get_if<std::string>(&item);
            if (!label || label->size() > 4096) throw std::runtime_error("Window choices need text items of at most 4096 bytes");
            (void)text_offsets(*label);
            if (!unique.insert(*label).second) throw std::runtime_error("Window choice items must be unique");
            widget.items.push_back(*label);
        }
        if (!widget.items.empty() && name != "checklist") { widget.choice = 0; widget.value = widget.items[0]; }
        widget.selected.assign(widget.items.size(), false);
    }
    auto* object = allocate<ObjectValue>(); object->window_control = true;
    auto property = [&](const std::string& key, Value value, Type type) { object->fields.push_back({key, {std::move(value), std::move(type), true, false}}); };
    property("caption", widget.text, {"text", nullptr, {}}); property("enabled", true, {"bool", nullptr, {}});
    if (name == "input" || (choice && name != "checklist")) property("value", name != "input" && widget.choice < 0 ? Value{} : Value{widget.value}, {"text", nullptr, {}});
    auto list_property = [&](const std::string& key, const std::vector<std::string>& values) {
        auto* list = allocate<ListValue>(); list->element = Type{"text", nullptr, {}};
        for (const auto& value : values) list->items.emplace_back(value);
        property(key, list, {"list", nullptr, std::make_shared<Type>(Type{"text", nullptr, {}})});
    };
    if (choice) list_property("items", widget.items);
    if (name == "checklist") list_property("selected", {});
    window.widgets().push_back(std::move(widget)); window_bindings.push_back({object, callback});
    return object;
}

Value Runtime::Impl::graphics_call(const std::string& name, const std::vector<Value>& args, const std::vector<Argument>& syntax, const NodePtr& site) {
    if (window.active()) throw std::runtime_error("Close the window before using the graphics library canvas");
    for (const auto& arg : syntax) if (arg.name) throw std::runtime_error("Graphics functions take positional arguments");
    auto count = [&](std::size_t min, std::size_t max) {
        if (args.size() < min || args.size() > max) throw std::runtime_error("graphics." + name + " received the wrong number of arguments");
    };
    auto text = [&](std::size_t index) -> const std::string& {
        const auto* value = std::get_if<std::string>(&args.at(index));
        if (!value) throw std::runtime_error("graphics." + name + " needs text for argument " + std::to_string(index + 1));
        return *value;
    };
    auto coordinate = [&](std::size_t index) {
        const auto value = number(args.at(index)).str(); float result = 0;
        const auto converted = std::from_chars(value.data(), value.data() + value.size(), result);
        if (converted.ec != std::errc{} || converted.ptr != value.data() + value.size() || !std::isfinite(result) || std::abs(result) > 10000000)
            throw std::runtime_error("Graphics coordinates must be between -10000000 and 10000000");
        return result;
    };
    auto path = [&](std::size_t index) {
        const auto& value = text(index);
        if (value.empty() || value.find('\0') != std::string::npos) throw std::runtime_error("Graphics path must be non-empty text without NUL characters");
        return path_text(std::filesystem::absolute((source ? source->directory : std::filesystem::current_path()) / source_path(value)).lexically_normal());
    };
    auto seconds = [&]() { return Number::divide(Number::integer(static_cast<std::size_t>(graphics.seconds_ns())), Number::integer(1000000000), 9); };
    if (name == "canvas") {
        count(2, 3);
        if (callback_running) throw std::runtime_error("Configure the canvas before graphics.run");
        const auto width = number(args[0]).size_value(4096), height = number(args[1]).size_value(4096);
        graphics.canvas(static_cast<int>(width), static_cast<int>(height), args.size() == 3 ? text(2) : "SLANG canvas"); return {};
    }
    if (name == "open" || name == "present") {
        count(0, 0);
        if (callback_running) throw std::runtime_error("graphics.run manages frames; do not call open or present inside its callback");
        if (name == "open") return graphics.open();
        const bool active = graphics.active(); graphics.present(); if (active) steps = 0; return {};
    }
    if (name == "seconds") { count(0, 0); return seconds(); }
    if (name == "close") { count(0, 0); graphics.close(); return {}; }
    if (name == "fps") { count(1, 1); graphics.fps(static_cast<int>(number(args[0]).size_value(240))); return {}; }
    if (name == "key_down") { count(1, 1); return graphics.key_down(text(0)); }
    if (name == "background" || name == "fill") {
        count(1, 1); if (name == "background") graphics.background(text(0)); else graphics.fill(text(0)); return {};
    }
    if (name == "line" || name == "rect") {
        count(4, 4);
        const auto x = coordinate(0), y = coordinate(1), a = coordinate(2), b = coordinate(3);
        if (name == "line") graphics.line(x,y,a,b); else graphics.rect(x,y,a,b); return {};
    }
    if (name == "circle") { count(3, 3); graphics.circle(coordinate(0), coordinate(1), coordinate(2)); return {}; }
    if (name == "sprite") {
        count(3, 5);
        if (args.size() == 4) throw std::runtime_error("graphics.sprite needs a path, x, y, and optionally both width and height");
        const auto file = path(0); const auto x = coordinate(1), y = coordinate(2);
        const auto width = args.size() == 5 ? coordinate(3) : -1, height = args.size() == 5 ? coordinate(4) : -1;
        if (args.size() == 5 && (width < 0 || height < 0)) throw std::runtime_error("Sprite dimensions cannot be negative");
        graphics.sprite(file, x, y, width, height); return {};
    }
    if (name == "save") { count(1, 1); graphics.save(path(0)); return {}; }
    if (name == "run") {
        count(1, 1);
        if (callback_running) throw std::runtime_error("Cannot nest graphics.run calls");
        const auto* callback = std::get_if<Callable*>(&args[0]);
        if (!callback || (*callback)->object) throw std::runtime_error("graphics.run needs a function accepting elapsed seconds");
        struct Running { bool& flag; explicit Running(bool& value) : flag(value) { flag = true; } ~Running() { flag = false; } } running(callback_running);
        while (graphics.open()) {
            tick(site);
            const std::vector<Value> arguments{seconds()};
            (void)invoke(*callback, arguments, {}, site);
            const bool active = graphics.active(); graphics.present(); if (active) steps = 0;
            maybe_collect();
        }
        return {};
    }
    throw std::runtime_error("Unknown graphics function '" + name + "'");
}

Value Runtime::Impl::invoke(Callable* callable, const std::vector<Value>& args, const std::vector<Argument>& syntax, const NodePtr& site) {
    Roots roots(*this); roots.add(callable); roots.add(args);
    if (callable->builtin.starts_with("graphics.")) return graphics_call(callable->builtin.substr(9), args, syntax, site);
    if (callable->builtin.starts_with("window.")) return window_call(callable->builtin.substr(7), args, syntax, site);
    if (callable->builtin.find('.') != std::string::npos) return library_call(callable->builtin, args, syntax, site);
    if (!callable->builtin.empty()) return builtin(callable->builtin, args, syntax);
    if (callable->object) {
        const auto& fields = get<Object>(callable->object)->fields;
        if (args.size() != fields.size()) throw std::runtime_error(callable->name + " needs " + std::to_string(fields.size()) + " field values");
        auto* object = allocate<ObjectValue>(); object->nominal = callable;
        for (std::size_t i = 0; i < fields.size(); ++i) {
            if (syntax[i].name) throw std::runtime_error("Declared object constructors take positional arguments");
            Cell cell; cell.type = annotation(fields[i].annotation, callable->closure); assign(cell, args[i], true);
            object->fields.push_back({fields[i].name, std::move(cell)});
        }
        return object;
    }
    FunctionValue* best = nullptr; int best_score = -1; bool ambiguous = false;
    for (auto& fn : callable->overloads) {
        if (fn.parameters.size() != args.size()) continue;
        int score = 0; bool match = true;
        for (std::size_t i = 0; i < args.size(); ++i) {
            if (!accepts(fn.parameters[i], args[i])) { match = false; break; }
            if (!fn.parameters[i].name.empty()) score += same_type(fn.parameters[i], value_type(args[i])) ? 2 : 1;
        }
        if (match && score > best_score) { best = &fn; best_score = score; ambiguous = false; }
        else if (match && score == best_score) ambiguous = true;
    }
    if (!best) throw std::runtime_error("No overload of '" + callable->name + "' accepts these argument types/count");
    if (ambiguous) throw std::runtime_error("Ambiguous overload of '" + callable->name + "'; use explicit conversions");
    for (const auto& arg : syntax) if (arg.name) throw std::runtime_error("Functions take positional arguments");
    if (calls >= 32) fail(site, "Function call depth limit reached (32 calls in this interpreter)", "R192");
    struct CallDepth { std::size_t& depth; explicit CallDepth(std::size_t& n) : depth(n) { ++depth; } ~CallDepth() { --depth; } } guard(calls);
    auto* env = environment(best->closure);
    const auto* function = get<Function>(best->syntax);
    std::vector<Type> argument_types;
    for (std::size_t i = 0; i < args.size(); ++i) {
        Cell cell; cell.type = best->parameters[i]; assign(cell, args[i], true);
        argument_types.push_back(cell.type);
        env->bindings[function->parameters[i].name] = std::move(cell);
    }
    struct RestoreSource { std::shared_ptr<Source>& current; std::shared_ptr<Source> previous; ~RestoreSource() { current = previous; } } restore{source, source};
    source = best->source;
    try {
        Value result;
        try { sequence(function->body, env); } catch (const Returned& returned) { result = returned.value; }
        auto found = std::find_if(best->returns.begin(), best->returns.end(), [&](const auto& specialization) {
            return std::equal(argument_types.begin(), argument_types.end(), specialization.arguments.begin(), specialization.arguments.end(), same_type);
        });
        if (found == best->returns.end()) { best->returns.push_back({argument_types, value_type(result)}); return result; }
        if (found->result.name.empty()) found->result = value_type(result);
        return coerce(found->result, result);
    } catch (Diagnostic& error) {
        if (error.source_name.empty() && source) { error.source_name = source->name; error.source_text = source->text; }
        throw;
    }
}

void Runtime::Impl::statement(const NodePtr& node, Environment* env, bool echo) {
    Roots roots(*this); roots.add(env);
    EvaluationDepth depth(evaluation_depth, node);
    tick(node);
    try {
        if (get<Function>(node) || get<Object>(node) || get<Public>(node) || get<Library>(node) || get<Import>(node)) return; // installed at block entry
        checkpoint(node, env);
        if (const auto* v = get<Variable>(node)) { assign(env->bindings.at(v->name), eval(v->value, env), true); return; }
        if (const auto* a = get<Assignment>(node)) { auto target = reference(a->target, env); roots.add(target.owner); auto value = eval(a->value, env); write(target, std::move(value)); return; }
        if (const auto* e = get<ExpressionStatement>(node)) { auto value = eval(e->expression, env); if (echo && !std::holds_alternative<std::monostate>(value)) output << display(value) << '\n'; return; }
        if (const auto* r = get<Return>(node)) throw Returned{eval(r->value, env)};
        if (get<Break>(node)) throw LoopControl{true};
        if (get<Continue>(node)) throw LoopControl{false};
        if (const auto* c = get<Conditional>(node)) {
            for (const auto& branch : c->branches) if (truth(eval(branch.condition, env))) { sequence(branch.body, environment(env)); return; }
            if (c->otherwise) sequence(*c->otherwise, environment(env));
            return;
        }
        if (const auto* w = get<While>(node)) {
            while (truth(eval(w->condition, env))) { if (!iteration(w->body, env)) break; checkpoint(node, env); }
            return;
        }
        if (const auto* r = get<Repeat>(node)) {
            const auto count = number(eval(r->count, env));
            if (!count.is_integer() || count.negative()) throw std::runtime_error("repeat needs a non-negative int count");
            for (auto i = Number::integer(0); Number::compare(i, count) < 0; i = Number::add(i, Number::integer(1))) { tick(node); if (!iteration(r->body, env)) break; }
            return;
        }
        if (const auto* f = get<ForRange>(node)) {
            auto from = number(eval(f->from, env)); auto to = number(eval(f->to, env));
            auto step = f->step ? number(eval(f->step, env)) : Number::integer(1);
            if (!from.is_integer() || !to.is_integer() || !step.is_integer() || step.zero()) throw std::runtime_error("Ranges need int endpoints and a non-zero int step");
            for (auto i = from; step.negative() ? Number::compare(i, to) >= 0 : Number::compare(i, to) <= 0; i = Number::add(i, step)) { tick(node); if (!iteration(f->body, env, f->variable, i)) break; }
            return;
        }
        if (const auto* f = get<ForEach>(node)) {
            auto values = eval(f->values, env);
            const auto* list = std::get_if<ListValue*>(&values);
            if (!list) throw std::runtime_error("for ... in/blant needs a list");
            const auto snapshot = (*list)->items;
            roots.add(snapshot);
            for (const auto& value : snapshot) { tick(node); if (!iteration(f->body, env, f->variable, value)) break; }
            return;
        }
        throw std::runtime_error("Unsupported statement");
    } catch (const Diagnostic&) { throw; }
    catch (const std::exception& error) { fail(node, error.what()); }
}

Runtime::Runtime(std::istream& input, std::ostream& output, std::function<bool()> interrupted, std::size_t max_steps)
    : impl_(std::make_unique<Impl>(input, output, std::move(interrupted), max_steps)) {}
Runtime::~Runtime() = default;
void Runtime::execute(const NodePtr& program, bool echo_expressions, std::string source_text, std::string source_name) {
    auto directory = std::filesystem::current_path();
    if (!source_name.empty() && source_name != "-" && source_name.front() != '<') directory = std::filesystem::absolute(source_path(source_name)).parent_path();
    impl_->source = std::make_shared<Source>(Source{std::move(source_text), std::move(source_name), directory});
    impl_->execute(program, echo_expressions);
}
void Runtime::reset() { impl_->reset(); }
bool Runtime::read_line(std::string& line) { return impl_->read_line(line); }
void Runtime::wait_for_graphics() { impl_->graphics.wait(); }
void Runtime::set_host(RuntimeHost host) { impl_->graphics.set_host(host.graphics); impl_->host = std::move(host); }
} // namespace slang
