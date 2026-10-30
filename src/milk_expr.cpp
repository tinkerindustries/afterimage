#include "milk_expr.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace Milk {

namespace {

// projectm-eval's two tolerances: band, bor and sigmoid use the wide one, and every other truth
// test, equality and division guard the narrow one.
constexpr double CLOSE = 0.00001;
constexpr double CLOSE_LOW = 1e-300;
constexpr int64_t MAX_LOOP = 1 << 20;
// Added to a memory index before it is truncated, so 2.9999999 reaches slot 3.
constexpr double MEM_ROUND = 0.0001;

bool truthy(double x) { return std::fabs(x) > CLOSE_LOW; }

double safeDiv(double a, double b) { return std::fabs(b) < CLOSE_LOW ? 0.0 : a / b; }

// pow with projectm-eval's guards: zero to a negative power and NaN results are 0.
double safePow(double a, double b) {
    if (std::fabs(a) < CLOSE_LOW && b < 0.0) return 0.0;
    const double r = std::pow(a, b);
    return std::isnan(r) ? 0.0 : r;
}

double intMod(double a, double b) {
    const int64_t d = static_cast<int64_t>(b);
    if (d == 0) return 0.0;
    return static_cast<double>(static_cast<int64_t>(a) % d);
}

int64_t bits(double x) { return static_cast<int64_t>(x); }

enum class Kind {
    Const, Var, Mem, GMem,
    Assign,
    Neg, Not,
    Add, Sub, Mul, Div, Mod, Pow,
    Less, Greater, LessEq, GreaterEq, Equal, NotEqual,
    And, Or, BitAnd, BitOr,
    If, Seq, Loop, While,
    Fn1, Fn2, Rand, MemCopy, MemFill,
};

} // namespace

double* MemoryBuffer::slot(size_t i) {
    std::unique_ptr<double[]>& block = m_blocks[i / BLOCK];
    if (!block) block = std::make_unique<double[]>(BLOCK);
    return &block[i % BLOCK];
}

double* MemoryBuffer::at(double index) {
    const double s = index + MEM_ROUND;
    if (!(s > -1.0 && s < static_cast<double>(SIZE))) return nullptr;
    return slot(static_cast<size_t>(static_cast<int32_t>(s)));
}

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
    // projectm-eval passes values by reference, so a variable, a memory slot, or an if, sequence
    // or assignment that ends in one can be assigned to. Anything else is evaluated into
    // `scratch`, and an assignment to it changes nothing but its own result.
    double* ref(double& scratch) const;
};

double* ExprNode::ref(double& scratch) const {
    switch (kind) {
    case Kind::Var: return var;
    case Kind::Mem:
    case Kind::GMem: {
        MemoryBuffer& buffer = kind == Kind::Mem ? vars->megabuf() : vars->globals().gmegabuf;
        double* p = buffer.at(kids[0]->eval());
        if (p) return p;
        scratch = 0.0;
        return &scratch;
    }
    case Kind::If:
        return kids[0]->eval() != 0.0 ? kids[1]->ref(scratch) : kids[2]->ref(scratch);
    case Kind::Seq:
        for (size_t i = 0; i + 1 < kids.size(); ++i) kids[i]->eval();
        return kids.back()->ref(scratch);
    case Kind::Assign: {
        double* target = kids[0]->ref(scratch);
        const double rhs = kids[1]->eval();
        switch (assignOp) {
        case '+': *target += rhs; break;
        case '-': *target -= rhs; break;
        case '*': *target *= rhs; break;
        case '/': *target = safeDiv(*target, rhs); break;
        case '%': *target = intMod(*target, rhs); break;
        case '^': *target = safePow(*target, rhs); break;
        case '&': *target = static_cast<double>(bits(*target) & bits(rhs)); break;
        case '|': *target = static_cast<double>(bits(*target) | bits(rhs)); break;
        default: *target = rhs; break;
        }
        return target;
    }
    default:
        scratch = eval();
        return &scratch;
    }
}

double ExprNode::eval() const {
    switch (kind) {
    case Kind::Const: return value;
    case Kind::Var: return *var;
    case Kind::Mem:
    case Kind::GMem:
    case Kind::Assign: {
        double scratch = 0.0;
        return *ref(scratch);
    }
    case Kind::Neg: return -kids[0]->eval();
    case Kind::Not: return std::fabs(kids[0]->eval()) < CLOSE_LOW ? 1.0 : 0.0;
    case Kind::Add: return kids[0]->eval() + kids[1]->eval();
    case Kind::Sub: return kids[0]->eval() - kids[1]->eval();
    case Kind::Mul: return kids[0]->eval() * kids[1]->eval();
    case Kind::Div: return safeDiv(kids[0]->eval(), kids[1]->eval());
    case Kind::Mod: return intMod(kids[0]->eval(), kids[1]->eval());
    case Kind::Pow: return safePow(kids[0]->eval(), kids[1]->eval());
    case Kind::Less: return kids[0]->eval() < kids[1]->eval() ? 1.0 : 0.0;
    case Kind::Greater: return kids[0]->eval() > kids[1]->eval() ? 1.0 : 0.0;
    case Kind::LessEq: return kids[0]->eval() <= kids[1]->eval() ? 1.0 : 0.0;
    case Kind::GreaterEq: return kids[0]->eval() >= kids[1]->eval() ? 1.0 : 0.0;
    case Kind::Equal: return std::fabs(kids[0]->eval() - kids[1]->eval()) < CLOSE_LOW ? 1.0 : 0.0;
    case Kind::NotEqual: return std::fabs(kids[0]->eval() - kids[1]->eval()) > CLOSE_LOW ? 1.0 : 0.0;
    // && and || skip their right side when the left decides. A NaN left side counts as true for
    // || and false for &&, as projectm-eval's comparisons give.
    case Kind::And:
        if (!(std::fabs(kids[0]->eval()) > CLOSE_LOW)) return 0.0;
        return truthy(kids[1]->eval()) ? 1.0 : 0.0;
    case Kind::Or:
        if (!(std::fabs(kids[0]->eval()) < CLOSE_LOW)) return 1.0;
        return truthy(kids[1]->eval()) ? 1.0 : 0.0;
    case Kind::BitAnd: return static_cast<double>(bits(kids[0]->eval()) & bits(kids[1]->eval()));
    case Kind::BitOr: return static_cast<double>(bits(kids[0]->eval()) | bits(kids[1]->eval()));
    case Kind::If: return kids[0]->eval() != 0.0 ? kids[1]->eval() : kids[2]->eval();
    case Kind::Seq: {
        double last = 0.0;
        for (const auto& k : kids) last = k->eval();
        return last;
    }
    case Kind::Loop: {
        const double count = kids[0]->eval();
        const int64_t n = std::isnan(count) ? 0 : static_cast<int64_t>(std::clamp(count, -1.0, static_cast<double>(MAX_LOOP)));
        double last = 0.0;
        for (int64_t i = 0; i < n; ++i) last = kids[1]->eval();
        return last;
    }
    case Kind::While: {
        int64_t n = MAX_LOOP;
        double last;
        do {
            last = kids[0]->eval();
        } while (std::fabs(last) > CLOSE_LOW && --n);
        return last;
    }
    case Kind::MemCopy:
    case Kind::MemFill: {
        const int64_t total = static_cast<int64_t>(MemoryBuffer::SIZE);
        const double first = kids[0]->eval();
        const double second = kids[1]->eval();
        int64_t count = static_cast<int64_t>(kids[2]->eval() + MEM_ROUND);
        int64_t dest = static_cast<int64_t>(first + MEM_ROUND);
        int64_t src = static_cast<int64_t>(second + MEM_ROUND);
        MemoryBuffer& buffer = vars->megabuf();
        if (kind == Kind::MemFill) {
            if (dest < 0) { count += dest; dest = 0; }
            count = std::min(count, total - dest);
            for (int64_t i = 0; i < count; ++i) *buffer.slot(static_cast<size_t>(dest + i)) = second;
            return first;
        }
        if (dest < 0) { count += dest; src -= dest; dest = 0; }
        if (src < 0) { count += src; dest -= src; src = 0; }
        count = std::min({count, total - dest, total - src});
        if (count <= 0) return first;
        // Overlapping ranges copy as memmove does.
        if (dest < src) {
            for (int64_t i = 0; i < count; ++i) *buffer.slot(static_cast<size_t>(dest + i)) = *buffer.slot(static_cast<size_t>(src + i));
        } else {
            for (int64_t i = count - 1; i >= 0; --i) *buffer.slot(static_cast<size_t>(dest + i)) = *buffer.slot(static_cast<size_t>(src + i));
        }
        return first;
    }
    case Kind::Fn1: return fn1(kids[0]->eval());
    case Kind::Fn2: return fn2(kids[0]->eval(), kids[1]->eval());
    case Kind::Rand: {
        // A fraction in [0, floor(x)), with x below 1 counted as 1.
        const double x = std::max(std::floor(kids[0]->eval()), 1.0);
        return static_cast<double>(vars->globals().rand()) * (1.0 / 4294967295.0) * x;
    }
    }
    return 0.0;
}

namespace {

// Function tables. Each takes and returns doubles the way projectm-eval's built-ins do.
double fnSqr(double x) { return x * x; }
double fnSqrt(double x) { return std::sqrt(std::fabs(x)); }
// MilkDrop's fast inverse square root: a bit trick and one Newton step, good to about 0.2%.
double fnInvSqrt(double x) {
    uint64_t raw;
    std::memcpy(&raw, &x, sizeof(raw));
    raw = 0x5fe6eb50c7b537a9ULL - (raw >> 1);
    double y;
    std::memcpy(&y, &raw, sizeof(y));
    y = y * (1.5 - (x * 0.5) * y * y);
    return std::isnan(y) ? 0.0 : y;
}
double fnSign(double x) { return x > 0.0 ? 1.0 : (x < 0.0 ? -1.0 : 0.0); }
double fnBnot(double x) { return std::fabs(x) < CLOSE_LOW ? 1.0 : 0.0; }
double fnNeg(double x) { return -x; }
double fnLog(double x) { return x > 0.0 ? std::log(x) : 0.0; }
double fnLog10(double x) { return x > 0.0 ? std::log10(x) : 0.0; }
double fnAbove(double a, double b) { return a > b ? 1.0 : 0.0; }
double fnBelow(double a, double b) { return a < b ? 1.0 : 0.0; }
double fnAboveEq(double a, double b) { return a >= b ? 1.0 : 0.0; }
double fnBelowEq(double a, double b) { return a <= b ? 1.0 : 0.0; }
double fnEqual(double a, double b) { return std::fabs(a - b) < CLOSE_LOW ? 1.0 : 0.0; }
double fnNotEqual(double a, double b) { return std::fabs(a - b) > CLOSE_LOW ? 1.0 : 0.0; }
double fnBand(double a, double b) { return std::fabs(a) > CLOSE && std::fabs(b) > CLOSE ? 1.0 : 0.0; }
double fnBor(double a, double b) { return std::fabs(a) > CLOSE || std::fabs(b) > CLOSE ? 1.0 : 0.0; }
// With a NaN argument these return the second, as projectm-eval's comparisons do.
double fnMin(double a, double b) { return a < b ? a : b; }
double fnMax(double a, double b) { return a > b ? a : b; }
double fnPow(double a, double b) { return safePow(a, b); }
double fnAtan2(double a, double b) { return std::atan2(a, b); }
double fnAdd(double a, double b) { return a + b; }
double fnSub(double a, double b) { return a - b; }
double fnMul(double a, double b) { return a * b; }
double fnDiv(double a, double b) { return safeDiv(a, b); }
double fnMod(double a, double b) { return intMod(a, b); }
double fnSigmoid(double x, double c) {
    const double t = 1.0 + std::exp(-x * c);
    return std::fabs(t) > CLOSE ? 1.0 / t : 0.0;
}
double fnSin(double x) { return std::sin(x); }
double fnCos(double x) { return std::cos(x); }
double fnTan(double x) { return std::tan(x); }
double fnAsin(double x) { return x < -1.0 || x > 1.0 ? 0.0 : std::asin(x); }
double fnAcos(double x) { return x < -1.0 || x > 1.0 ? 0.0 : std::acos(x); }
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
    {"floor", fnFloor}, {"ceil", fnCeil}, {"bnot", fnBnot}, {"_not", fnBnot}, {"_neg", fnNeg},
};

const Fn2Entry FN2[] = {
    {"atan2", fnAtan2}, {"pow", fnPow}, {"min", fnMin}, {"max", fnMax}, {"above", fnAbove}, {"_above", fnAbove},
    {"below", fnBelow}, {"_below", fnBelow}, {"_aboeq", fnAboveEq}, {"_beleq", fnBelowEq}, {"equal", fnEqual},
    {"_equal", fnEqual}, {"_noteq", fnNotEqual}, {"band", fnBand}, {"bor", fnBor}, {"sigmoid", fnSigmoid},
    {"_add", fnAdd}, {"_sub", fnSub}, {"_mul", fnMul}, {"_div", fnDiv}, {"_mod", fnMod},
};

// Names projectm-eval reads as functions. Any of them used without '(' is a syntax error.
const char* OTHER_FUNCTIONS[] = {
    "if", "_if", "_and", "_or", "loop", "while", "_set", "assign", "_mulop", "_divop", "_orop", "_andop",
    "_addop", "_subop", "_modop", "_powop", "rand", "exec2", "exec3", "_mem", "megabuf", "_gmem", "gmegabuf",
    "freembuf", "memcpy", "memset",
};

bool isFunctionName(const std::string& name) {
    for (const Fn1Entry& f : FN1) {
        if (name == f.name) return true;
    }
    for (const Fn2Entry& f : FN2) {
        if (name == f.name) return true;
    }
    for (const char* f : OTHER_FUNCTIONS) {
        if (name == f) return true;
    }
    return false;
}

struct Token {
    enum Type { NUM, IDENT, FUNC, GMEM, OP, END } type = END;
    std::string text;
    double value = 0.0;
};

class ParseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

bool digit(char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; }

// projectm-eval's scanner. A number is digits with at most one '.' and an optional exponent, and
// needs at least one digit, so a lone '.' is an error and "1.2.3" is two numbers.
std::vector<Token> tokenise(const std::string& src) {
    static const char* MULTI[] = {"+=", "-=", "*=", "/=", "%=", "^=", "&=", "|=", "==", "!=", "<=", ">=", "&&", "||"};
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
        if (digit(c) || (c == '.' && i + 1 < n && digit(src[i + 1]))) {
            size_t j = i;
            while (j < n && digit(src[j])) ++j;
            if (j < n && src[j] == '.') {
                ++j;
                while (j < n && digit(src[j])) ++j;
            }
            if (j < n && (src[j] == 'e' || src[j] == 'E')) {
                size_t k = j + 1;
                if (k < n && (src[k] == '+' || src[k] == '-')) ++k;
                if (k < n && digit(src[k])) {
                    while (k < n && digit(src[k])) ++k;
                    j = k;
                }
            }
            Token t{Token::NUM};
            t.value = std::strtod(src.substr(i, j - i).c_str(), nullptr);
            out.push_back(t);
            i = j;
            continue;
        }
        if (c == '$' && i + 3 < n && src[i + 1] == '\'' && src[i + 3] == '\'') {
            Token t{Token::NUM};
            t.value = static_cast<signed char>(src[i + 2]);
            out.push_back(t);
            i += 4;
            continue;
        }
        if (c == '$') {
            // $pi, $e and $phi are single-precision literals in projectm-eval.
            size_t j = i + 1;
            if (j < n && (src[j] == 'x' || src[j] == 'X') && j + 1 < n && std::isxdigit(static_cast<unsigned char>(src[j + 1]))) {
                size_t k = j + 1;
                while (k < n && std::isxdigit(static_cast<unsigned char>(src[k]))) ++k;
                Token t{Token::NUM};
                t.value = static_cast<double>(std::strtoul(src.substr(j + 1, k - j - 1).c_str(), nullptr, 16));
                out.push_back(t);
                i = k;
                continue;
            }
            auto word = [&](const char* w) {
                const size_t len = std::strlen(w);
                if (j + len > n) return false;
                for (size_t k = 0; k < len; ++k) {
                    if (std::tolower(static_cast<unsigned char>(src[j + k])) != w[k]) return false;
                }
                return true;
            };
            Token t{Token::NUM};
            if (word("phi")) { t.value = static_cast<double>(1.61803399f); i = j + 3; }
            else if (word("pi")) { t.value = static_cast<double>(3.141592653589793f); i = j + 2; }
            else if (word("e")) { t.value = static_cast<double>(2.71828183f); i = j + 1; }
            else throw ParseError("unknown constant after '$'");
            out.push_back(t);
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t j = i;
            while (j < n && (std::isalnum(static_cast<unsigned char>(src[j])) || src[j] == '_')) ++j;
            Token t{Token::IDENT, src.substr(i, j - i)};
            std::transform(t.text.begin(), t.text.end(), t.text.begin(), ::tolower);
            if (t.text == "gmem") t.type = Token::GMEM;
            else if (isFunctionName(t.text)) t.type = Token::FUNC;
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
        if (std::string("+-*/%^=<>!&|?:(),;[]").find(c) != std::string::npos) {
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
        if (peek().type == Token::END) return makeNode(Kind::Const);
        NodePtr node = parseList();
        if (peek().type != Token::END) throw ParseError("unexpected '" + peek().text + "'");
        return node;
    }

private:
    const Token& peek(size_t ahead = 0) const { return m_tokens[std::min(m_pos + ahead, m_tokens.size() - 1)]; }
    bool isOp(const char* op, size_t ahead = 0) const { return peek(ahead).type == Token::OP && peek(ahead).text == op; }
    bool accept(const char* op) {
        if (!isOp(op)) return false;
        ++m_pos;
        return true;
    }
    void expect(const char* op) {
        if (!accept(op)) throw ParseError(std::string("expected '") + op + "'");
    }

    // An instruction list: expressions separated by ';'. It starts with an expression, and after a
    // ';' an empty statement or "()" is allowed.
    NodePtr parseList() {
        NodePtr seq = makeNode(Kind::Seq);
        seq->kids.push_back(parseExpression());
        while (accept(";")) {
            if (peek().type == Token::END || isOp(")") || isOp(",") || isOp(";")) continue;
            if (isOp("(") && isOp(")", 1)) {
                m_pos += 2;
                continue;
            }
            seq->kids.push_back(parseExpression());
        }
        if (seq->kids.size() == 1) return std::move(seq->kids[0]);
        return seq;
    }

    NodePtr parseExpression() { return parseAssign(0); }

    // The assignment operators bind loosest, in four right-associative levels: = then += -= then
    // *= /= %= then ^= |= &=. So "a += b = c" is "(a += b) = c".
    static int assignLevel(const Token& t) {
        if (t.type != Token::OP) return -1;
        if (t.text == "=") return 0;
        if (t.text == "+=" || t.text == "-=") return 1;
        if (t.text == "*=" || t.text == "/=" || t.text == "%=") return 2;
        if (t.text == "^=" || t.text == "|=" || t.text == "&=") return 3;
        return -1;
    }

    NodePtr parseAssign(int minLevel) {
        NodePtr lhs = parseTernary();
        while (true) {
            const int level = assignLevel(peek());
            if (level < minLevel) return lhs;
            const char op = peek().text[0];
            ++m_pos;
            NodePtr node = makeBinary(Kind::Assign, std::move(lhs), parseAssign(level));
            node->assignOp = op;
            lhs = std::move(node);
        }
    }

    // ?: binds tighter than assignment and looser than ||. Its middle runs to the ':', and its
    // last part is another ternary at most, so "c ? x = 1 : x = 2" is "(c ? x = 1 : x) = 2".
    NodePtr parseTernary() {
        NodePtr cond = parseOr();
        if (!accept("?")) return cond;
        NodePtr node = makeNode(Kind::If);
        node->kids.push_back(std::move(cond));
        node->kids.push_back(parseExpression());
        expect(":");
        node->kids.push_back(parseTernary());
        return node;
    }

    // From here down the levels follow projectm-eval's grammar, loosest first: || && | & == > < +
    // * % ! ^ and then unary minus. Note that | and & bind looser than comparisons, % binds
    // tighter than * and /, ^ is left associative, and unary minus binds tighter than ^ so -a^b is
    // (-a)^b.
    NodePtr parseOr() {
        NodePtr lhs = parseAnd();
        while (accept("||")) lhs = makeBinary(Kind::Or, std::move(lhs), parseAnd());
        return lhs;
    }

    NodePtr parseAnd() {
        NodePtr lhs = parseBitOr();
        while (accept("&&")) lhs = makeBinary(Kind::And, std::move(lhs), parseBitOr());
        return lhs;
    }

    NodePtr parseBitOr() {
        NodePtr lhs = parseBitAnd();
        while (accept("|")) lhs = makeBinary(Kind::BitOr, std::move(lhs), parseBitAnd());
        return lhs;
    }

    NodePtr parseBitAnd() {
        NodePtr lhs = parseEquality();
        while (accept("&")) lhs = makeBinary(Kind::BitAnd, std::move(lhs), parseEquality());
        return lhs;
    }

    NodePtr parseEquality() {
        NodePtr lhs = parseGreater();
        while (true) {
            if (accept("==")) lhs = makeBinary(Kind::Equal, std::move(lhs), parseGreater());
            else if (accept("!=")) lhs = makeBinary(Kind::NotEqual, std::move(lhs), parseGreater());
            else return lhs;
        }
    }

    NodePtr parseGreater() {
        NodePtr lhs = parseLess();
        while (true) {
            if (accept(">")) lhs = makeBinary(Kind::Greater, std::move(lhs), parseLess());
            else if (accept(">=")) lhs = makeBinary(Kind::GreaterEq, std::move(lhs), parseLess());
            else return lhs;
        }
    }

    NodePtr parseLess() {
        NodePtr lhs = parseAdd();
        while (true) {
            if (accept("<")) lhs = makeBinary(Kind::Less, std::move(lhs), parseAdd());
            else if (accept("<=")) lhs = makeBinary(Kind::LessEq, std::move(lhs), parseAdd());
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
        NodePtr lhs = parseMod();
        while (true) {
            if (accept("*")) lhs = makeBinary(Kind::Mul, std::move(lhs), parseMod());
            else if (accept("/")) lhs = makeBinary(Kind::Div, std::move(lhs), parseMod());
            else return lhs;
        }
    }

    NodePtr parseMod() {
        NodePtr lhs = parseNot();
        while (accept("%")) lhs = makeBinary(Kind::Mod, std::move(lhs), parseNot());
        return lhs;
    }

    NodePtr parseNot() {
        if (accept("!")) {
            NodePtr node = makeNode(Kind::Not);
            node->kids.push_back(parseNot());
            return node;
        }
        return parsePower();
    }

    NodePtr parsePower() {
        NodePtr lhs = parseUnary();
        while (accept("^")) lhs = makeBinary(Kind::Pow, std::move(lhs), parseUnary());
        return lhs;
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
        return parsePostfix();
    }

    NodePtr memory(Kind kind, NodePtr index) {
        NodePtr node = makeNode(kind);
        node->vars = &m_vars;
        node->kids.push_back(std::move(index));
        return node;
    }

    // `index[]` and `base[offset]` address megabuf, so `10000[5]` is megabuf(10005).
    NodePtr parsePostfix() {
        NodePtr node = parsePrimary();
        while (accept("[")) {
            NodePtr index = std::move(node);
            if (!accept("]")) {
                NodePtr offset = parseExpression();
                expect("]");
                index = makeBinary(Kind::Add, std::move(index), std::move(offset));
            }
            node = memory(Kind::Mem, std::move(index));
        }
        return node;
    }

    // Each argument is an instruction list. A call needs at least one argument.
    std::vector<NodePtr> parseArgs() {
        std::vector<NodePtr> args;
        while (true) {
            args.push_back(parseList());
            if (accept(")")) return args;
            expect(",");
        }
    }

    NodePtr parseCall(const std::string& name) {
        expect("(");
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
        static const struct { const char* name; char op; } COMPOUND[] = {
            {"_set", '='}, {"assign", '='}, {"_addop", '+'}, {"_subop", '-'}, {"_mulop", '*'}, {"_divop", '/'},
            {"_modop", '%'}, {"_powop", '^'}, {"_orop", '|'}, {"_andop", '&'},
        };
        for (const auto& c : COMPOUND) {
            if (name == c.name) {
                need(2);
                NodePtr node = makeNode(Kind::Assign);
                node->assignOp = c.op;
                node->kids = std::move(args);
                return node;
            }
        }
        NodePtr node;
        if (name == "if" || name == "_if") { need(3); node = makeNode(Kind::If); }
        else if (name == "_and") { need(2); node = makeNode(Kind::And); }
        else if (name == "_or") { need(2); node = makeNode(Kind::Or); }
        else if (name == "loop") { need(2); node = makeNode(Kind::Loop); }
        else if (name == "while") { need(1); node = makeNode(Kind::While); }
        else if (name == "rand") { need(1); node = makeNode(Kind::Rand); }
        else if (name == "megabuf" || name == "_mem") { need(1); node = makeNode(Kind::Mem); }
        else if (name == "gmegabuf" || name == "_gmem") { need(1); node = makeNode(Kind::GMem); }
        else if (name == "memcpy") { need(3); node = makeNode(Kind::MemCopy); }
        else if (name == "memset") { need(3); node = makeNode(Kind::MemFill); }
        else if (name == "exec2" || name == "exec3") { need(name == "exec2" ? 2 : 3); node = makeNode(Kind::Seq); }
        else if (name == "freembuf") {
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
        if (t.type == Token::GMEM) {
            ++m_pos;
            expect("[");
            NodePtr index = isOp("]") ? makeNode(Kind::Const) : parseExpression();
            expect("]");
            return memory(Kind::GMem, std::move(index));
        }
        if (t.type == Token::FUNC) {
            const std::string name = t.text;
            ++m_pos;
            if (!isOp("(")) throw ParseError(name + " is a function");
            return parseCall(name);
        }
        if (t.type == Token::IDENT) {
            ++m_pos;
            if (isOp("(")) throw ParseError("unknown function " + t.text + "()");
            NodePtr node = makeNode(Kind::Var);
            node->var = m_vars.slot(t.text);
            return node;
        }
        if (accept("(")) {
            NodePtr node = parseList();
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
    return false;
}

void Program::run() const {
    if (m_root) m_root->eval();
}

} // namespace Milk
