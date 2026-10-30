#pragma once

#include <cstdint>
#include <deque>
#include <memory>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

namespace Milk {

// megabuf and gmegabuf: 2^23 doubles in blocks of 65536, each allocated on first use. Addresses
// stay valid once taken, so an assignment can hold its target while its right side runs.
class MemoryBuffer {
public:
    static constexpr size_t SIZE = size_t(1) << 23;
    // The slot for an index, rounded as ns-eel rounds it, or null outside the buffer.
    double* at(double index);
    double* slot(size_t i);

private:
    static constexpr size_t BLOCK = size_t(1) << 16;
    std::vector<std::unique_ptr<double[]>> m_blocks = std::vector<std::unique_ptr<double[]>>(SIZE / BLOCK);
};

// State every expression context shares: reg00..reg99, gmegabuf and the random generator.
struct ExprGlobals {
    std::vector<double> regs = std::vector<double>(100, 0.0);
    MemoryBuffer gmegabuf;
    // MilkDrop's Mersenne Twister and its fixed seed.
    std::mt19937 rand{0x4141f00du};
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
    MemoryBuffer& megabuf() { return m_megabuf; }

private:
    ExprGlobals& m_globals;
    std::unordered_map<std::string, double*> m_names;
    std::deque<double> m_values;
    MemoryBuffer m_megabuf;
};

struct ExprNode;

// A compiled block of MilkDrop expression code (ns-eel2 syntax).
class Program {
public:
    Program();
    Program(Program&&) noexcept;
    Program& operator=(Program&&) noexcept;
    ~Program();

    // Compiles `source` against `vars` with projectm-eval's grammar, which Milkwave 4.1 uses. A
    // block with any statement that fails to parse is dropped whole, as MilkDrop drops it. Returns
    // false then, and `error` names the problem.
    bool compile(const std::string& source, VarTable& vars, std::string& error);
    bool empty() const { return m_root == nullptr; }
    void run() const;

private:
    std::unique_ptr<ExprNode> m_root;
};

} // namespace Milk
