#pragma once
#include "slang/ast.hpp"
#include "slang/host.hpp"
#include <functional>
#include <iosfwd>
#include <memory>

namespace slang {
// A persistent native interpreter session. ASTs remain alive for closures.
// Strong type checks currently occur during execution, not in a static compiler.
class Runtime {
public:
    Runtime(std::istream& input, std::ostream& output,
        std::function<bool()> interrupted = {}, std::size_t max_steps = 1000000);
    ~Runtime();
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    void execute(const NodePtr& program, bool echo_expressions = false,
        std::string source_text = {}, std::string source_name = {});
    void reset();
    bool read_line(std::string& line);
    void wait_for_graphics();
    void set_host(RuntimeHost host);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace slang
