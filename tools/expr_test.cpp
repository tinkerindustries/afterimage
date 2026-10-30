// Checks the expression engine against projectm-eval's behaviour, which Milkwave 4.1 uses. Exits non-zero on a failure.

#include "milk_expr.h"

#include <cmath>
#include <cstdio>

using namespace Milk;

namespace {

int failures = 0;

void check(const char* code, double expect, double tolerance = 1e-4) {
    ExprGlobals globals;
    VarTable vars(globals);
    Program program;
    std::string error;
    const bool ok = program.compile(code, vars, error);
    program.run();
    const double got = vars.get("r");
    if (!ok || std::fabs(got - expect) > tolerance) {
        ++failures;
        std::printf("FAIL %-48s got %.6f, expected %.6f %s\n", code, got, expect, ok ? "" : error.c_str());
    }
}

void checkRejected(const char* code) {
    ExprGlobals globals;
    VarTable vars(globals);
    Program program;
    std::string error;
    if (program.compile(code, vars, error)) {
        ++failures;
        std::printf("FAIL %-48s compiled but should not\n", code);
    }
}

} // namespace

int main() {
    // Operator precedence follows projectm-eval: unary minus binds tighter than ^, ^ is left associative,
    // % binds tighter than *, and | and & bind looser than comparisons.
    check("r = -2^2;", 4);
    check("r = 2^3^2;", 64);
    check("r = 2*7%4;", 2 * (7 % 4));
    check("r = 1+2==3;", 1);
    check("r = 1==1 & 2==2;", 1);
    check("r = 6 & 3 == 3;", 0);
    check("r = !0+1;", 2);
    check("r = !1^2;", 0);
    check("r = 3 > 2 > 1;", 0);
    check("r = 1 || 0 && 0;", 1);

    // Guards.
    check("r = 5/0;", 0);
    check("r = 5/0.000001;", 5000000);
    check("r = asin(2) + acos(-1.5);", 0);
    check("r = min(exp(1000) - exp(1000), 3);", 3);
    check("r = log(-3);", 0);
    check("r = log10(0);", 0);
    check("r = pow(0,-1);", 0);
    check("r = 0^-2;", 0);
    check("r = (-8)^(1/3);", 0);
    check("r = sqrt(-4);", 2);
    check("r = 7 % 3;", 1);
    check("r = -7 % 3;", -1);
    check("r = 7 % 0;", 0);

    // Operators and constants.
    check("r = 5; r &= 3;", 1);
    check("r = 4; r |= 3;", 7);
    check("r = $'a';", 97);
    check("r = $pi;", 3.14159265);
    check("r = $xff;", 255);
    check("r = int(-1.5);", -2);
    check("r = invsqrt(4);", 0.5, 0.005);
    check("r = sigmoid(0,1);", 0.5);
    check("r = min(3,2)+max(1,5);", 7);
    check("r = above(3,2)+below(3,2)+equal(2,2);", 2);
    check("r = 1 ? 5 : 6;", 5);
    check("x=0; 1 ? x=3 : x=4; r=x;", 4);

    // Memory.
    check("x=3; 10000[5]=x; r=10000[5];", 3);
    check("10005[]=7; r=megabuf(10005);", 7);
    check("gmem[42]=9; r=gmegabuf(42);", 9);
    check("megabuf(7000000)=4; r=megabuf(7000000);", 4);
    check("r = megabuf(9000000);", 0);
    check("memset(10,5,4); r=megabuf(12);", 5);
    check("memset(10,5,4); memcpy(20,10,4); r=megabuf(23);", 5);

    // Loops.
    check("r = 0; loop(5, r = r + 1);", 5);
    check("n=0; while(n=n+1; n<10); r=n;", 10);

    // rand(x) is a fraction in [0, floor(x)) from MilkDrop's Mersenne Twister, seeded 0x4141f00d.
    check("r = rand(1);", 0.227726, 1e-6);
    check("r = rand(0.5) < 1 && rand(4) >= 0 && rand(4) < 4;", 1);
    check("r=0; loop(100, x=rand(10); r=r+(x!=int(x))); r=r;", 100);

    // A block with one statement that fails to compile is dropped whole.
    {
        ExprGlobals globals;
        VarTable vars(globals);
        Program program;
        std::string error;
        program.compile("r = 3; s = (1;", vars, error);
        program.run();
        if (!program.empty() || vars.get("r") != 0.0) {
            ++failures;
            std::printf("FAIL a block with a bad statement still ran\n");
        }
    }

    // Truth tests and equality are exact; band and bor allow 1e-5.
    check("r = 1 == 1.000001;", 0);
    check("r = 1 != 1.000001;", 1);
    check("r = !0.000001;", 0);
    check("r = if(0.000001, 5, 6);", 5);
    check("r = band(0.000001, 1);", 0);

    // Assignment binds below ?:, and ?:, sequences and assignments can be assigned to.
    check("x = 0; 1 ? x = 1 : x = 2; r = x;", 2);
    check("a = 0; b = 0; (0 ? a : b) = 5; r = b;", 5);
    check("a = 1; a += 2 = 7; r = a;", 7);
    check("r = (5 = 3);", 3);
    check("x = 3; r = (x *= 2) + 1;", 7);
    check("r = 0; ; r = 4;", 4);
    check("r = 1; ();", 1);

    // Syntax that must not compile.
    checkRejected("r = fmod(5,3);");
    checkRejected("r = .;");
    checkRejected("r = 0x1f;");
    checkRejected("r = 1.2.3;");
    checkRejected("; r = 1;");
    checkRejected("min = 1;");
    checkRejected("r = 1 ? 2;");
    checkRejected("r = rand();");

    std::printf("%d failures\n", failures);
    return failures == 0 ? 0 : 1;
}
