#include "milk_expr.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <stdexcept>

namespace Milk {

namespace {

constexpr double EPSILON = 0.00001;
constexpr int MAX_LOOP = 1 << 20;
constexpr size_t MEGABUF_SIZE = 1 << 20;

bool truthy(double x) { return std::fabs(x) > EPSILON; }

double safeDiv(double a, double b) { return b == 0.0 ? 0.0 : a / b; }

double intMod(double a, double b) {
    const long long d = static_cast<long long>(std::fabs(b));
    if (d == 0) return 0.0;
    return static_cast<double>(static_cast<long long>(a) % d);
}

double nextRand(ExprGlobals& g) {
    g.randState ^= g.randState << 13;
    g.randState ^= g.randState >> 7;
    g.randState ^= g.randState << 17;
    return static_cast<double>(g.randState >> 11) / static_cast<double>(1ULL << 53);
}

enum class Kind {
    Const, Var, Mem, GMem,
    Assign,
    Neg, Not,
    Add, Sub, Mul, Div, Mod, Pow,
    Less, Greater, LessEq, GreaterEq, Equal, NotEqual,
    And, Or, BitAnd, BitOr,
    If, Seq, Loop, While,
    Fn1, Fn2, Rand,
};

} // namespace

struct ExprNode {
    Kind kind = Kind::Const;
    double value = 0.0;
    double* var = nullptr;
    VarTable* vars = nullptr;
    char assignOp = '=';
    double (*fn1)(double) = nullptr;
    double (*fn2)(double, double) = nullptr;
    std::vector<std::unique_ptr<ExprNode>> kids;

    double eval() const;
    double* ref() const;
};

namespace {

double* memRef(std::vector<double>& buffer, double index) {
    if (!(index >= 0.0) || index >= static_cast<double>(MEGABUF_SIZE)) return nullptr;
    const size_t i = static_cast<size_t>(index + EPSILON);
    if (i >= buffer.size()) buffer.resize(std::min(MEGABUF_SIZE, std::max(i + 1, buffer.size() * 2)), 0.0);
    return &buffer[i];
}

} // namespace

double* ExprNode::ref() const {
    switch (kind) {
    case Kind::Var: return var;
    case Kind::Mem: return memRef(vars->megabuf(), kids[0]->eval());
    case Kind::GMem: return memRef(vars->globals().gmegabuf, kids[0]->eval());
    default: return nullptr;
    }
}

double ExprNode::eval() const {
    switch (kind) {
    case Kind::Const: return value;
    case Kind::Var: return *var;
    case Kind::Mem:
    case Kind::GMem: {
        const double* p = ref();
        return p ? *p : 0.0;
    }
    case Kind::Assign: {
        const double rhs = kids[1]->eval();
        double* target = kids[0]->ref();
        if (target == nullptr) return rhs;
        switch (assignOp) {
        case '+': *target += rhs; break;
        case '-': *target -= rhs; break;
        case '*': *target *= rhs; break;
        case '/': *target = safeDiv(*target, rhs); break;
        case '%': *target = intMod(*target, rhs); break;
        case '^': *target = std::pow(*target, rhs); break;
        default: *target = rhs; break;
        }
        return *target;
    }
    case Kind::Neg: return -kids[0]->eval();
    case Kind::Not: return truthy(kids[0]->eval()) ? 0.0 : 1.0;
    case Kind::Add: return kids[0]->eval() + kids[1]->eval();
    case Kind::Sub: return kids[0]->eval() - kids[1]->eval();
    case Kind::Mul: return kids[0]->eval() * kids[1]->eval();
    case Kind::Div: return safeDiv(kids[0]->eval(), kids[1]->eval());
    case Kind::Mod: return intMod(kids[0]->eval(), kids[1]->eval());
    case Kind::Pow: return std::pow(kids[0]->eval(), kids[1]->eval());
    case Kind::Less: return kids[0]->eval() < kids[1]->eval() ? 1.0 : 0.0;
    case Kind::Greater: return kids[0]->eval() > kids[1]->eval() ? 1.0 : 0.0;
    case Kind::LessEq: return kids[0]->eval() <= kids[1]->eval() ? 1.0 : 0.0;
    case Kind::GreaterEq: return kids[0]->eval() >= kids[1]->eval() ? 1.0 : 0.0;
    case Kind::Equal: return std::fabs(kids[0]->eval() - kids[1]->eval()) < EPSILON ? 1.0 : 0.0;
    case Kind::NotEqual: return std::fabs(kids[0]->eval() - kids[1]->eval()) < EPSILON ? 0.0 : 1.0;
    case Kind::And: return truthy(kids[0]->eval()) && truthy(kids[1]->eval()) ? 1.0 : 0.0;
    case Kind::Or: return truthy(kids[0]->eval()) || truthy(kids[1]->eval()) ? 1.0 : 0.0;
    case Kind::BitAnd:
        return static_cast<double>(static_cast<long long>(kids[0]->eval()) & static_cast<long long>(kids[1]->eval()));
    case Kind::BitOr:
        return static_cast<double>(static_cast<long long>(kids[0]->eval()) | static_cast<long long>(kids[1]->eval()));
    case Kind::If:
        if (truthy(kids[0]->eval())) return kids[1]->eval();
        return kids.size() > 2 ? kids[2]->eval() : 0.0;
    case Kind::Seq: {
        double last = 0.0;
        for (const auto& k : kids) last = k->eval();
        return last;
    }
    case Kind::Loop: {
        const int n = std::min(MAX_LOOP, static_cast<int>(kids[0]->eval()));
        double last = 0.0;
        for (int i = 0; i < n; ++i) last = kids[1]->eval();
        return last;
    }
    case Kind::While: {
        int n = 0;
        while (truthy(kids[0]->eval()) && ++n < MAX_LOOP) {}
        return 0.0;
    }
    case Kind::Fn1: return fn1(kids[0]->eval());
    case Kind::Fn2: return fn2(kids[0]->eval(), kids[1]->eval());
    case Kind::Rand: {
        const double x = kids[0]->eval();
        const double r = nextRand(vars->globals());
        return x >= 1.0 ? std::floor(r * std::floor(x)) : r * x;
    }
    }
    return 0.0;
}

namespace {

// Function tables. Each takes and returns doubles the way ns-eel2's built-ins do.
double fnSqr(double x) { return x * x; }
double fnSqrt(double x) { return std::sqrt(std::fabs(x)); }
double fnInvSqrt(double x) { return x == 0.0 ? 0.0 : 1.0 / std::sqrt(std::fabs(x)); }
double fnSign(double x) { return x > 0.0 ? 1.0 : (x < 0.0 ? -1.0 : 0.0); }
double fnBnot(double x) { return truthy(x) ? 0.0 : 1.0; }
double fnLog(double x) { return x > 0.0 ? std::log(x) : -1e30; }
double fnLog10(double x) { return x > 0.0 ? std::log10(x) : -1e30; }
double fnAbove(double a, double b) { return a > b ? 1.0 : 0.0; }
double fnBelow(double a, double b) { return a < b ? 1.0 : 0.0; }
double fnEqual(double a, double b) { return std::fabs(a - b) < EPSILON ? 1.0 : 0.0; }
double fnBand(double a, double b) { return truthy(a) && truthy(b) ? 1.0 : 0.0; }
double fnBor(double a, double b) { return truthy(a) || truthy(b) ? 1.0 : 0.0; }
double fnMin(double a, double b) { return std::min(a, b); }
double fnMax(double a, double b) { return std::max(a, b); }
double fnPow(double a, double b) { return std::pow(a, b); }
double fnFmod(double a, double b) { return b == 0.0 ? 0.0 : std::fmod(a, b); }
double fnAtan2(double a, double b) { return std::atan2(a, b); }
double fnSigmoid(double x, double c) {
    const double t = 1.0 + std::exp(-x * c);
    return std::fabs(t) > EPSILON ? 1.0 / t : 0.0;
}
double fnSin(double x) { return std::sin(x); }
double fnCos(double x) { return std::cos(x); }
double fnTan(double x) { return std::tan(x); }
double fnAsin(double x) { return std::asin(x); }
double fnAcos(double x) { return std::acos(x); }
double fnAtan(double x) { return std::atan(x); }
double fnExp(double x) { return std::exp(x); }
double fnAbs(double x) { return std::fabs(x); }
double fnFloor(double x) { return std::floor(x); }
double fnCeil(double x) { return std::ceil(x); }

struct Fn1Entry { const char* name; double (*fn)(double); };
struct Fn2Entry { const char* name; double (*fn)(double, double); };

const Fn1Entry FN1[] = {
    {"sin", fnSin}, {"cos", fnCos}, {"tan", fnTan}, {"asin", fnAsin}, {"acos", fnAcos},
    {"atan", fnAtan}, {"sqr", fnSqr}, {"sqrt", fnSqrt}, {"invsqrt", fnInvSqrt}, {"exp", fnExp},
    {"log", fnLog}, {"log10", fnLog10}, {"abs", fnAbs}, {"sign", fnSign}, {"int", fnFloor},
    {"floor", fnFloor}, {"ceil", fnCeil}, {"bnot", fnBnot},
};

const Fn2Entry FN2[] = {
    {"atan2", fnAtan2}, {"pow", fnPow}, {"min", fnMin}, {"max", fnMax}, {"above", fnAbove},
    {"below", fnBelow}, {"equal", fnEqual}, {"band", fnBand}, {"bor", fnBor},
    {"sigmoid", fnSigmoid}, {"fmod", fnFmod},
};

struct Token {
    enum Type { NUM, IDENT, OP, END } type = END;
    std::string text;
    double value = 0.0;
};

class ParseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

std::vector<Token> tokenise(const std::string& src) {
    static const char* MULTI[] = {"+=", "-=", "*=", "/=", "%=", "^=", "==", "!=", "<=", ">=", "&&", "||"};
    std::vector<Token> out;
    size_t i = 0;
    const size_t n = src.size();
    while (i < n) {
        const char c = src[i];
        if (std::isspace(static_cast<unsigned char>(c))) { ++i; continue; }
        if (c == '/' && i + 1 < n && src[i + 1] == '/') {
            while (i < n && src[i] != '\n') ++i;
            continue;
        }
        if (c == '/' && i + 1 < n && src[i + 1] == '*') {
            const size_t end = src.find("*/", i + 2);
            i = end == std::string::npos ? n : end + 2;
            continue;
        }
        // ns-eel reads a lone '.' as 0, which some presets rely on ("below(rad,.-53)").
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            Token t{Token::NUM};
            if (c == '0' && i + 1 < n && (src[i + 1] == 'x' || src[i + 1] == 'X')) {
                size_t j = i + 2;
                while (j < n && std::isxdigit(static_cast<unsigned char>(src[j]))) ++j;
                t.value = static_cast<double>(std::stoull(src.substr(i + 2, j - i - 2), nullptr, 16));
                i = j;
            } else {
                size_t j = i;
                while (j < n && (std::isdigit(static_cast<unsigned char>(src[j])) || src[j] == '.')) ++j;
                if (j < n && (src[j] == 'e' || src[j] == 'E') && j + 1 < n &&
                    (std::isdigit(static_cast<unsigned char>(src[j + 1])) || ((src[j + 1] == '-' || src[j + 1] == '+') && j + 2 < n && std::isdigit(static_cast<unsigned char>(src[j + 2]))))) {
                    j += 2;
                    while (j < n && std::isdigit(static_cast<unsigned char>(src[j]))) ++j;
                }
                t.value = std::strtod(src.substr(i, j - i).c_str(), nullptr);
                i = j;
            }
            out.push_back(t);
            continue;
        }
        if (c == '$') {
            size_t j = i + 1;
            while (j < n && (std::isalnum(static_cast<unsigned char>(src[j])) || src[j] == '_')) ++j;
            std::string name = src.substr(i + 1, j - i - 1);
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            Token t{Token::NUM};
            if (name == "pi") t.value = 3.141592653589793;
            else if (name == "e") t.value = 2.718281828459045;
            else if (name == "phi") t.value = 1.618033988749895;
            else if (name.size() > 1 && name[0] == 'x') t.value = static_cast<double>(std::stoull(name.substr(1), nullptr, 16));
            else throw ParseError("unknown constant $" + name);
            out.push_back(t);
            i = j;
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t j = i;
            while (j < n && (std::isalnum(static_cast<unsigned char>(src[j])) || src[j] == '_')) ++j;
            Token t{Token::IDENT, src.substr(i, j - i)};
            std::transform(t.text.begin(), t.text.end(), t.text.begin(), ::tolower);
            out.push_back(t);
            i = j;
            continue;
        }
        bool matched = false;
        for (const char* op : MULTI) {
            if (src.compare(i, 2, op) == 0) {
                out.push_back(Token{Token::OP, op});
                i += 2;
                matched = true;
                break;
            }
        }
        if (matched) continue;
        if (std::string("+-*/%^=<>!&|?:(),;").find(c) != std::string::npos) {
            out.push_back(Token{Token::OP, std::string(1, c)});
            ++i;
            continue;
        }
        throw ParseError(std::string("unexpected character '") + c + "'");
    }
    out.push_back(Token{Token::END});
    return out;
}

using NodePtr = std::unique_ptr<ExprNode>;

NodePtr makeNode(Kind kind) {
    auto node = std::make_unique<ExprNode>();
    node->kind = kind;
    return node;
}

NodePtr makeBinary(Kind kind, NodePtr a, NodePtr b) {
    NodePtr node = makeNode(kind);
    node->kids.push_back(std::move(a));
    node->kids.push_back(std::move(b));
    return node;
}

class Parser {
public:
    Parser(const std::vector<Token>& tokens, VarTable& vars) : m_tokens(tokens), m_vars(vars) {}

    NodePtr parseAll() {
        NodePtr node = parseSequence();
        if (peek().type != Token::END) throw ParseError("unexpected '" + peek().text + "'");
        return node;
    }

private:
    const Token& peek() const { return m_tokens[m_pos]; }
    bool isOp(const char* op) const { return peek().type == Token::OP && peek().text == op; }
    bool accept(const char* op) {
        if (!isOp(op)) return false;
        ++m_pos;
        return true;
    }
    void expect(const char* op) {
        if (!accept(op)) throw ParseError(std::string("expected '") + op + "'");
    }

    // Statements separated by ';', ending at ')', ',' or the end. Empty statements are allowed.
    NodePtr parseSequence() {
        NodePtr seq = makeNode(Kind::Seq);
        while (true) {
            if (accept(";")) continue;
            if (peek().type == Token::END || isOp(")") || isOp(",")) break;
            seq->kids.push_back(parseAssign());
            if (!accept(";")) break;
        }
        if (seq->kids.empty()) return makeNode(Kind::Const);
        if (seq->kids.size() == 1) return std::move(seq->kids[0]);
        return seq;
    }

    NodePtr parseAssign() {
        NodePtr lhs = parseTernary();
        static const char* OPS[] = {"=", "+=", "-=", "*=", "/=", "%=", "^="};
        for (const char* op : OPS) {
            if (isOp(op)) {
                if (lhs->kind != Kind::Var && lhs->kind != Kind::Mem && lhs->kind != Kind::GMem) {
                    throw ParseError("assignment to something that is not a variable");
                }
                ++m_pos;
                NodePtr node = makeBinary(Kind::Assign, std::move(lhs), parseAssign());
                node->assignOp = op[0];
                return node;
            }
        }
        return lhs;
    }

    NodePtr parseTernary() {
        NodePtr cond = parseOr();
        if (!accept("?")) return cond;
        NodePtr node = makeNode(Kind::If);
        node->kids.push_back(std::move(cond));
        node->kids.push_back(parseAssign());
        if (accept(":")) node->kids.push_back(parseAssign());
        return node;
    }

    NodePtr parseOr() {
        NodePtr lhs = parseAnd();
        while (accept("||")) lhs = makeBinary(Kind::Or, std::move(lhs), parseAnd());
        return lhs;
    }

    NodePtr parseAnd() {
        NodePtr lhs = parseCompare();
        while (accept("&&")) lhs = makeBinary(Kind::And, std::move(lhs), parseCompare());
        return lhs;
    }

    NodePtr parseCompare() {
        NodePtr lhs = parseBits();
        while (true) {
            Kind kind;
            if (accept("<")) kind = Kind::Less;
            else if (accept(">")) kind = Kind::Greater;
            else if (accept("<=")) kind = Kind::LessEq;
            else if (accept(">=")) kind = Kind::GreaterEq;
            else if (accept("==")) kind = Kind::Equal;
            else if (accept("!=")) kind = Kind::NotEqual;
            else return lhs;
            lhs = makeBinary(kind, std::move(lhs), parseBits());
        }
    }

    NodePtr parseBits() {
        NodePtr lhs = parseAdd();
        while (true) {
            if (accept("|")) lhs = makeBinary(Kind::BitOr, std::move(lhs), parseAdd());
            else if (accept("&")) lhs = makeBinary(Kind::BitAnd, std::move(lhs), parseAdd());
            else return lhs;
        }
    }

    NodePtr parseAdd() {
        NodePtr lhs = parseMul();
        while (true) {
            if (accept("+")) lhs = makeBinary(Kind::Add, std::move(lhs), parseMul());
            else if (accept("-")) lhs = makeBinary(Kind::Sub, std::move(lhs), parseMul());
            else return lhs;
        }
    }

    NodePtr parseMul() {
        NodePtr lhs = parseUnary();
        while (true) {
            if (accept("*")) lhs = makeBinary(Kind::Mul, std::move(lhs), parseUnary());
            else if (accept("/")) lhs = makeBinary(Kind::Div, std::move(lhs), parseUnary());
            else if (accept("%")) lhs = makeBinary(Kind::Mod, std::move(lhs), parseUnary());
            else return lhs;
        }
    }

    NodePtr parseUnary() {
        if (accept("-")) {
            NodePtr node = makeNode(Kind::Neg);
            node->kids.push_back(parseUnary());
            return node;
        }
        if (accept("+")) return parseUnary();
        if (accept("!")) {
            NodePtr node = makeNode(Kind::Not);
            node->kids.push_back(parseUnary());
            return node;
        }
        return parsePow();
    }

    NodePtr parsePow() {
        NodePtr base = parsePrimary();
        if (accept("^")) return makeBinary(Kind::Pow, std::move(base), parseUnary());
        return base;
    }

    std::vector<NodePtr> parseArgs() {
        std::vector<NodePtr> args;
        if (accept(")")) return args;
        while (true) {
            args.push_back(parseSequence());
            if (accept(")")) return args;
            expect(",");
        }
    }

    NodePtr parseCall(const std::string& name) {
        std::vector<NodePtr> args = parseArgs();
        auto need = [&](size_t count) {
            if (args.size() != count) {
                throw ParseError(name + " takes " + std::to_string(count) + " arguments, given " + std::to_string(args.size()));
            }
        };
        for (const Fn1Entry& f : FN1) {
            if (name == f.name) {
                need(1);
                NodePtr node = makeNode(Kind::Fn1);
                node->fn1 = f.fn;
                node->kids = std::move(args);
                return node;
            }
        }
        for (const Fn2Entry& f : FN2) {
            if (name == f.name) {
                need(2);
                NodePtr node = makeNode(Kind::Fn2);
                node->fn2 = f.fn;
                node->kids = std::move(args);
                return node;
            }
        }
        NodePtr node;
        if (name == "if") { need(3); node = makeNode(Kind::If); }
        else if (name == "loop") { need(2); node = makeNode(Kind::Loop); }
        else if (name == "while") { need(1); node = makeNode(Kind::While); }
        else if (name == "rand") { need(1); node = makeNode(Kind::Rand); }
        else if (name == "megabuf") { need(1); node = makeNode(Kind::Mem); }
        else if (name == "gmegabuf") { need(1); node = makeNode(Kind::GMem); }
        else if (name == "exec2" || name == "exec3") { need(name == "exec2" ? 2 : 3); node = makeNode(Kind::Seq); }
        else if (name == "assign") {
            need(2);
            if (args[0]->kind != Kind::Var && args[0]->kind != Kind::Mem && args[0]->kind != Kind::GMem) {
                throw ParseError("assign() needs a variable first");
            }
            node = makeNode(Kind::Assign);
        } else if (name == "freembuf") {
            need(1);
            return makeNode(Kind::Const);
        } else {
            throw ParseError("unknown function " + name + "()");
        }
        node->vars = &m_vars;
        node->kids = std::move(args);
        return node;
    }

    NodePtr parsePrimary() {
        const Token& t = peek();
        if (t.type == Token::NUM) {
            ++m_pos;
            NodePtr node = makeNode(Kind::Const);
            node->value = t.value;
            return node;
        }
        if (t.type == Token::IDENT) {
            const std::string name = t.text;
            ++m_pos;
            if (accept("(")) return parseCall(name);
            NodePtr node = makeNode(Kind::Var);
            node->var = m_vars.slot(name);
            return node;
        }
        if (accept("(")) {
            NodePtr node = parseSequence();
            expect(")");
            return node;
        }
        if (t.type == Token::END) throw ParseError("unexpected end of code");
        throw ParseError("unexpected '" + t.text + "'");
    }

    const std::vector<Token>& m_tokens;
    size_t m_pos = 0;
    VarTable& m_vars;
};

NodePtr compileSource(const std::string& source, VarTable& vars) {
    const std::vector<Token> tokens = tokenise(source);
    Parser parser(tokens, vars);
    return parser.parseAll();
}

// Splits at ';' outside brackets so each statement can be compiled on its own.
std::vector<std::string> splitStatements(const std::string& source) {
    std::vector<std::string> out;
    std::string current;
    int depth = 0;
    for (size_t i = 0; i < source.size(); ++i) {
        const char c = source[i];
        if (c == '/' && i + 1 < source.size() && source[i + 1] == '/') {
            while (i < source.size() && source[i] != '\n') ++i;
            continue;
        }
        if (c == '(') ++depth;
        if (c == ')') depth = std::max(0, depth - 1);
        if (c == ';' && depth == 0) {
            out.push_back(current);
            current.clear();
            continue;
        }
        current += c;
    }
    out.push_back(current);
    return out;
}

} // namespace

double* VarTable::slot(const std::string& name) {
    auto it = m_names.find(name);
    if (it != m_names.end()) return it->second;
    double* p = nullptr;
    if (name.size() == 5 && name.compare(0, 3, "reg") == 0 && std::isdigit(static_cast<unsigned char>(name[3])) &&
        std::isdigit(static_cast<unsigned char>(name[4]))) {
        p = &m_globals.regs[(name[3] - '0') * 10 + (name[4] - '0')];
    } else {
        m_values.push_back(0.0);
        p = &m_values.back();
    }
    m_names.emplace(name, p);
    return p;
}

Program::Program() = default;
Program::Program(Program&&) noexcept = default;
Program& Program::operator=(Program&&) noexcept = default;
Program::~Program() = default;

bool Program::compile(const std::string& source, VarTable& vars, std::string& error) {
    m_root.reset();
    error.clear();
    try {
        m_root = compileSource(source, vars);
        if (m_root->kind == Kind::Const) m_root.reset();
        return true;
    } catch (const std::exception& e) {
        error = e.what();
    }
    NodePtr seq = makeNode(Kind::Seq);
    for (const std::string& statement : splitStatements(source)) {
        try {
            seq->kids.push_back(compileSource(statement, vars));
        } catch (const std::exception&) {
        }
    }
    if (!seq->kids.empty()) m_root = std::move(seq);
    return false;
}

void Program::run() const {
    if (m_root) m_root->eval();
}

} // namespace Milk
