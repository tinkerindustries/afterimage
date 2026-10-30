#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Milk {

// State every expression context shares: reg00..reg99, gmegabuf and the random generator.
struct ExprGlobals {
    std::vector<double> regs = std::vector<double>(100, 0.0);
    std::vector<double> gmegabuf;
    uint64_t randState = 0x9e3779b97f4a7c15ULL;
};

// Named variables for one block of preset code. Names are lower case. Addresses stay valid for
// the table's lifetime, so compiled code holds pointers into it.
class VarTable {
public:
    explicit VarTable(ExprGlobals& globals) : m_globals(globals) {}

    double* slot(const std::string& name);
    double get(const std::string& name) { return *slot(name); }
    void set(const std::string& name, double value) { *slot(name) = value; }

    ExprGlobals& globals() { return m_globals; }
    std::vector<double>& megabuf() { return m_megabuf; }

private:
    ExprGlobals& m_globals;
    std::unordered_map<std::string, double*> m_names;
    std::deque<double> m_values;
    std::vector<double> m_megabuf;
};

struct ExprNode;

// A compiled block of MilkDrop expression code (ns-eel2 syntax).
class Program {
public:
    Program();
    Program(Program&&) noexcept;
    Program& operator=(Program&&) noexcept;
    ~Program();

    // Compiles `source` against `vars`. When the whole block fails to parse, each top-level
    // statement is compiled on its own and the ones that parse are kept. Returns false if any
    // statement was dropped; `error` then names the first problem.
    bool compile(const std::string& source, VarTable& vars, std::string& error);
    bool empty() const { return m_root == nullptr; }
    void run() const;

private:
    std::unique_ptr<ExprNode> m_root;
};

} // namespace Milk
