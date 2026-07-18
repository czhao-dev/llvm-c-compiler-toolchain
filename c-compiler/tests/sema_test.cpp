#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "sema.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifndef MINIC_EXAMPLES_DIR
#define MINIC_EXAMPLES_DIR "examples"
#endif

namespace {

std::string readFile(const std::string &path) {
    std::ifstream file(path);
    assert(file && "could not open example file");
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::vector<minic::Diagnostic> analyzeSource(const std::string &source, const std::string &filename = "<input>") {
    minic::Lexer lexer(source, filename);
    minic::Parser parser(lexer.tokenize());
    const minic::ProgramNode program = parser.parseProgram();

    minic::SemanticAnalyzer analyzer;
    return analyzer.analyze(program);
}

std::vector<minic::Diagnostic> analyzeFile(const std::string &path) {
    return analyzeSource(readFile(path), path);
}

bool hasError(const std::vector<minic::Diagnostic> &diags, const std::string &substring) {
    for (const auto &diag : diags) {
        if (diag.severity == minic::DiagnosticSeverity::Error &&
            diag.message.find(substring) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool hasWarning(const std::vector<minic::Diagnostic> &diags, const std::string &substring) {
    for (const auto &diag : diags) {
        if (diag.severity == minic::DiagnosticSeverity::Warning &&
            diag.message.find(substring) != std::string::npos) {
            return true;
        }
    }
    return false;
}

int errorCount(const std::vector<minic::Diagnostic> &diags) {
    int count = 0;
    for (const auto &diag : diags) {
        count += diag.severity == minic::DiagnosticSeverity::Error ? 1 : 0;
    }
    return count;
}

} // namespace

int main() {
    const std::string examplesDir = MINIC_EXAMPLES_DIR;

    // Every example program is well-typed: zero diagnostics.
    for (const std::string &name : {"fibonacci.mc", "gcd.mc", "fizzbuzz.mc", "sum_of_squares.mc",
                                     "pointer_swap.mc", "array_sum.mc", "struct_point.mc", "bit_ops.mc",
                                     "control_flow.mc"}) {
        const auto diags = analyzeFile(examplesDir + "/" + name);
        if (!diags.empty()) {
            for (const auto &diag : diags) {
                std::cerr << name << ": " << diag.toString() << '\n';
            }
        }
        assert(diags.empty());
    }

    // Forward references: a function may call another defined later in the file.
    {
        const auto diags = analyzeSource(
            "int main() {\n"
            "    return helper();\n"
            "}\n"
            "\n"
            "int helper() {\n"
            "    return 42;\n"
            "}\n");
        assert(diags.empty());
    }

    // Use of an undeclared variable.
    {
        const auto diags = analyzeSource("int main() {\n    return nn;\n}\n", "bad.mc");
        assert(hasError(diags, "use of undeclared variable 'nn'"));
        assert(diags.front().location.line == 2);
    }

    // Call to an undeclared function.
    {
        const auto diags = analyzeSource("int main() {\n    return foo();\n}\n");
        assert(hasError(diags, "call to undeclared function 'foo'"));
    }

    // Wrong number of arguments to a call.
    {
        const auto diags = analyzeSource(
            "int add(int a, int b) {\n"
            "    return a + b;\n"
            "}\n"
            "\n"
            "int main() {\n"
            "    return add(1, 2, 3);\n"
            "}\n");
        assert(hasError(diags, "wrong number of arguments to 'add' \xe2\x80\x94 expected 2, got 3"));
    }

    // Redeclaration of a variable in the same scope.
    {
        const auto diags = analyzeSource(
            "int main() {\n"
            "    int x = 1;\n"
            "    int x = 2;\n"
            "    return x;\n"
            "}\n");
        assert(hasError(diags, "redefinition of 'x'"));
    }

    // A variable declared in a nested block does not leak into the outer scope.
    {
        const auto diags = analyzeSource(
            "int main() {\n"
            "    if (1) {\n"
            "        int y = 5;\n"
            "    }\n"
            "    return y;\n"
            "}\n");
        assert(hasError(diags, "use of undeclared variable 'y'"));
    }

    // A non-void function must return a value.
    {
        const auto diags = analyzeSource(
            "int f() {\n"
            "    return;\n"
            "}\n"
            "\n"
            "int main() {\n"
            "    return f();\n"
            "}\n");
        assert(hasError(diags, "non-void function 'f' must return a value"));
    }

    // A void function must not return a value.
    {
        const auto diags = analyzeSource(
            "void f() {\n"
            "    return 1;\n"
            "}\n"
            "\n"
            "int main() {\n"
            "    f();\n"
            "    return 0;\n"
            "}\n");
        assert(hasError(diags, "void function 'f' should not return a value"));
    }

    // Assigning an incompatible (non-numeric) type is an error.
    {
        const auto diags = analyzeSource(
            "void f() {}\n"
            "\n"
            "int main() {\n"
            "    int x = f();\n"
            "    return x;\n"
            "}\n");
        assert(hasError(diags, "cannot convert 'void' to 'int'"));
    }

    // Assigning a float to an int is a narrowing-conversion warning, not an error.
    {
        const auto diags = analyzeSource(
            "int main() {\n"
            "    int x = 1.5;\n"
            "    return x;\n"
            "}\n");
        assert(hasWarning(diags, "implicit conversion from 'float' to 'int' may lose precision"));
        assert(errorCount(diags) == 0);
    }

    // break/continue outside of a loop is an error.
    {
        const auto diags = analyzeSource(
            "int main() {\n"
            "    break;\n"
            "    return 0;\n"
            "}\n");
        assert(hasError(diags, "'break' statement not within a loop"));
    }

    {
        const auto diags = analyzeSource(
            "int main() {\n"
            "    continue;\n"
            "    return 0;\n"
            "}\n");
        assert(hasError(diags, "'continue' statement not within a loop"));
    }

    // break/continue inside a for-loop are fine.
    {
        const auto diags = analyzeSource(
            "int main() {\n"
            "    for (int i = 0; i < 10; i = i + 1) {\n"
            "        if (i == 5) { continue; }\n"
            "        if (i == 8) { break; }\n"
            "    }\n"
            "    return 0;\n"
            "}\n");
        assert(diags.empty());
    }

    // Pointers: address-of, dereference, assignment through a pointer, and
    // null-pointer-constant compatibility are all well-typed.
    {
        const auto diags = analyzeSource(
            "void swap(int *a, int *b) {\n"
            "    int temp = *a;\n"
            "    *a = *b;\n"
            "    *b = temp;\n"
            "}\n"
            "\n"
            "int main() {\n"
            "    int x = 1;\n"
            "    int *p = &x;\n"
            "    int *q = 0;\n"
            "    swap(&x, p);\n"
            "    if (p) {\n"
            "        q = p;\n"
            "    }\n"
            "    return *p;\n"
            "}\n");
        assert(diags.empty());
    }

    // Dereferencing a non-pointer type is an error.
    {
        const auto diags = analyzeSource(
            "int main() {\n"
            "    int x = 5;\n"
            "    return *x;\n"
            "}\n");
        assert(hasError(diags, "cannot dereference non-pointer type 'int'"));
    }

    // Taking the address of a non-lvalue is an error.
    {
        const auto diags = analyzeSource(
            "int main() {\n"
            "    int *p = &5;\n"
            "    return *p;\n"
            "}\n");
        assert(hasError(diags, "cannot take the address of a non-lvalue expression"));
    }

    // Assigning between pointers of different pointee types is an error.
    {
        const auto diags = analyzeSource(
            "int main() {\n"
            "    int x = 1;\n"
            "    float *p = &x;\n"
            "    return 0;\n"
            "}\n");
        assert(hasError(diags, "cannot convert 'int*' to 'float*'"));
    }

    // Dereferencing a pointer to an incomplete type (void) is an error.
    {
        const auto diags = analyzeSource(
            "int f(void *p) {\n"
            "    return *p;\n"
            "}\n"
            "\n"
            "int main() {\n"
            "    return 0;\n"
            "}\n");
        assert(hasError(diags, "cannot dereference pointer to incomplete type 'void'"));
    }

    // Arrays: declaration, indexed read/write, address-of an element, and
    // passing an array to a pointer parameter (array-to-pointer decay) are
    // all well-typed.
    {
        const auto diags = analyzeSource(
            "int sum(int *arr, int n) {\n"
            "    int total = 0;\n"
            "    int i = 0;\n"
            "    while (i < n) {\n"
            "        total = total + arr[i];\n"
            "        i = i + 1;\n"
            "    }\n"
            "    return total;\n"
            "}\n"
            "\n"
            "int main() {\n"
            "    int values[5];\n"
            "    values[0] = 1;\n"
            "    int *p = &values[0];\n"
            "    int *q = values;\n"
            "    return sum(values, 5);\n"
            "}\n");
        assert(diags.empty());
    }

    // Arrays are not assignable as a whole.
    {
        const auto diags = analyzeSource(
            "int main() {\n"
            "    int a[3];\n"
            "    int b[3];\n"
            "    a = b;\n"
            "    return 0;\n"
            "}\n");
        assert(hasError(diags, "array 'a' is not assignable"));
    }

    // Taking the address of an array (no pointer-to-array type) is an error.
    {
        const auto diags = analyzeSource(
            "int main() {\n"
            "    int a[3];\n"
            "    int **p = &a;\n"
            "    return 0;\n"
            "}\n");
        assert(hasError(diags, "cannot take the address of array 'a'"));
    }

    // Indexing a non-array, non-pointer type is an error.
    {
        const auto diags = analyzeSource(
            "int main() {\n"
            "    int x = 5;\n"
            "    return x[0];\n"
            "}\n");
        assert(hasError(diags, "subscripted value is not an array or pointer"));
    }

    // An array declared with a non-positive size is a syntax error, caught
    // at parse time (a Type with arrayLength 0 is indistinguishable from a
    // non-array, so this can't be a sema-level check).
    {
        bool threw = false;
        try {
            minic::Lexer lexer("int main() { int a[0]; return 0; }", "bad.mc");
            minic::Parser parser(lexer.tokenize());
            parser.parseProgram();
        } catch (const std::exception &ex) {
            threw = true;
            assert(std::string(ex.what()).find("array size must be a positive integer") != std::string::npos);
        }
        assert(threw);
    }

    // Structs/unions/enums: declaration, member access (read/write via . and
    // ->), by-value struct params/returns/assignment (copy semantics), enum
    // constants, and forward field references (B used by value in A, B
    // declared afterward) are all well-typed.
    {
        const auto diags = analyzeSource(
            "struct A { struct B b; int x; };\n"
            "struct B { int y; };\n"
            "\n"
            "enum Color { RED, GREEN, BLUE };\n"
            "\n"
            "union Number { int i; float f; };\n"
            "\n"
            "struct A makeA() {\n"
            "    struct A a;\n"
            "    a.x = 1;\n"
            "    a.b.y = 2;\n"
            "    return a;\n"
            "}\n"
            "\n"
            "void touch(struct A *p) {\n"
            "    p->x = p->x + 1;\n"
            "}\n"
            "\n"
            "int main() {\n"
            "    struct A a = makeA();\n"
            "    struct A copy = a;\n"
            "    touch(&copy);\n"
            "    int c = GREEN;\n"
            "    union Number n;\n"
            "    n.i = 5;\n"
            "    return a.x + copy.x + a.b.y + c + n.i;\n"
            "}\n");
        assert(diags.empty());
    }

    // A struct can't directly contain itself by value.
    {
        const auto diags = analyzeSource("struct Node { struct Node next; };\n"
                                          "int main() { return 0; }\n");
        assert(hasError(diags, "would have infinite size"));
    }

    // ...but a pointer to itself is fine (the standard linked-structure idiom).
    {
        const auto diags = analyzeSource("struct Node { struct Node *next; int val; };\n"
                                          "int main() { return 0; }\n");
        assert(diags.empty());
    }

    // Referencing an undeclared struct tag is an error.
    {
        const auto diags = analyzeSource("int main() {\n"
                                          "    struct Foo p;\n"
                                          "    return 0;\n"
                                          "}\n");
        assert(hasError(diags, "use of undeclared struct 'Foo'"));
    }

    // Accessing a field that doesn't exist is an error.
    {
        const auto diags = analyzeSource("struct Point { int x; int y; };\n"
                                          "int main() {\n"
                                          "    struct Point p;\n"
                                          "    return p.z;\n"
                                          "}\n");
        assert(hasError(diags, "no member named 'z' in 'struct Point'"));
    }

    // Member access on a non-struct/union type is an error.
    {
        const auto diags = analyzeSource("int main() {\n"
                                          "    int x = 5;\n"
                                          "    return x.y;\n"
                                          "}\n");
        assert(hasError(diags, "is not a struct or union"));
    }

    // struct/union/enum tags share one namespace.
    {
        const auto diags = analyzeSource("struct Foo { int x; };\n"
                                          "union Foo { int y; };\n"
                                          "int main() { return 0; }\n");
        assert(hasError(diags, "redefinition of 'Foo'"));
    }

    // Bitwise/ternary/inc-dec/compound-assignment: well-typed cases.
    {
        const auto diags = analyzeSource(
            "int main() {\n"
            "    int a = 5;\n"
            "    int b = a & 1 | a ^ 2;\n"
            "    int c = ~a << 1 >> 1;\n"
            "    int d = a > b ? a : b;\n"
            "    a++;\n"
            "    --a;\n"
            "    a += 1;\n"
            "    a &= 1;\n"
            "    int *p = &a;\n"
            "    if (!p) { return 0; }\n"
            "    int e = (a++, b++, a + b);\n"
            "    return b + c + d + e;\n"
            "}\n");
        assert(diags.empty());
    }

    // Bitwise operators require integral (not float) operands.
    {
        const auto diags = analyzeSource("int main() {\n"
                                          "    float f = 1.5;\n"
                                          "    int x = f & 1;\n"
                                          "    return x;\n"
                                          "}\n");
        assert(hasError(diags, "invalid operands to binary '&'"));
    }

    // '~' requires an integral (not float) operand.
    {
        const auto diags = analyzeSource("int main() {\n"
                                          "    float f = 1.5;\n"
                                          "    return ~f;\n"
                                          "}\n");
        assert(hasError(diags, "invalid operand to unary '~'"));
    }

    // ++/-- require an lvalue operand.
    {
        const auto diags = analyzeSource("int main() {\n"
                                          "    int x = (1 + 2)++;\n"
                                          "    return x;\n"
                                          "}\n");
        assert(hasError(diags, "operand of '++' is not assignable"));
    }

    // The two branches of a ternary must be compatible.
    {
        const auto diags = analyzeSource("struct P { int x; };\n"
                                          "int main() {\n"
                                          "    struct P p;\n"
                                          "    int y = 1 ? p : 5;\n"
                                          "    return y;\n"
                                          "}\n");
        assert(hasError(diags, "incompatible operand types in ternary expression"));
    }

    // Compound assignment on a pointer with a non-pointer operator is an
    // error (no pointer arithmetic yet).
    {
        const auto diags = analyzeSource("int main() {\n"
                                          "    int x = 5;\n"
                                          "    int *p = &x;\n"
                                          "    p += 1;\n"
                                          "    return 0;\n"
                                          "}\n");
        assert(hasError(diags, "invalid operands to binary '+'"));
    }

    // do-while, switch (with fallthrough), break inside a switch, and
    // goto/labels (forward and backward) are all well-typed.
    {
        const auto diags = analyzeSource(
            "int main() {\n"
            "    int i = 0;\n"
            "    do {\n"
            "        i++;\n"
            "    } while (i < 3);\n"
            "\n"
            "    switch (i) {\n"
            "    case 0:\n"
            "    case 1:\n"
            "        i = 1;\n"
            "        break;\n"
            "    default:\n"
            "        i = 2;\n"
            "    }\n"
            "\n"
            "    int n = 0;\n"
            "top:\n"
            "    n++;\n"
            "    if (n < 3) {\n"
            "        goto top;\n"
            "    }\n"
            "    goto bottom;\n"
            "bottom:\n"
            "    return n;\n"
            "}\n");
        assert(diags.empty());
    }

    // do-while's condition is checked in the outer scope, not the body's.
    {
        const auto diags = analyzeSource("int main() {\n"
                                          "    do {\n"
                                          "        int x = 1;\n"
                                          "    } while (x > 0);\n"
                                          "    return 0;\n"
                                          "}\n");
        assert(hasError(diags, "use of undeclared variable 'x'"));
    }

    // A switch's value must be an integer type.
    {
        const auto diags = analyzeSource("int main() {\n"
                                          "    float f = 1.5;\n"
                                          "    switch (f) {\n"
                                          "    case 0:\n"
                                          "        return 0;\n"
                                          "    }\n"
                                          "    return 1;\n"
                                          "}\n");
        assert(hasError(diags, "switch value must have an integer type"));
    }

    // Duplicate case values are an error.
    {
        const auto diags = analyzeSource("int main() {\n"
                                          "    switch (1) {\n"
                                          "    case 1:\n"
                                          "        return 0;\n"
                                          "    case 1:\n"
                                          "        return 1;\n"
                                          "    }\n"
                                          "    return 2;\n"
                                          "}\n");
        assert(hasError(diags, "duplicate case value '1'"));
    }

    // More than one 'default' label is an error.
    {
        const auto diags = analyzeSource("int main() {\n"
                                          "    switch (1) {\n"
                                          "    default:\n"
                                          "        return 0;\n"
                                          "    default:\n"
                                          "        return 1;\n"
                                          "    }\n"
                                          "}\n");
        assert(hasError(diags, "multiple 'default' labels"));
    }

    // 'break' now works inside a switch even with no enclosing loop.
    {
        const auto diags = analyzeSource("int main() {\n"
                                          "    switch (1) {\n"
                                          "    case 1:\n"
                                          "        break;\n"
                                          "    }\n"
                                          "    return 0;\n"
                                          "}\n");
        assert(diags.empty());
    }

    // 'continue' inside a switch with no enclosing loop is still an error
    // (a switch is not a loop for 'continue' purposes).
    {
        const auto diags = analyzeSource("int main() {\n"
                                          "    switch (1) {\n"
                                          "    case 1:\n"
                                          "        continue;\n"
                                          "    }\n"
                                          "    return 0;\n"
                                          "}\n");
        assert(hasError(diags, "'continue' statement not within a loop"));
    }

    // goto to an undeclared label is an error.
    {
        const auto diags = analyzeSource("int main() {\n"
                                          "    goto nowhere;\n"
                                          "    return 0;\n"
                                          "}\n");
        assert(hasError(diags, "use of undeclared label 'nowhere'"));
    }

    // Duplicate labels in one function are an error.
    {
        const auto diags = analyzeSource("int main() {\n"
                                          "top:\n"
                                          "    return 0;\n"
                                          "top:\n"
                                          "    return 1;\n"
                                          "}\n");
        assert(hasError(diags, "duplicate label 'top'"));
    }

    // Redefinition of a function.
    {
        const auto diags = analyzeSource(
            "int f() { return 0; }\n"
            "int f() { return 1; }\n"
            "\n"
            "int main() {\n"
            "    return f();\n"
            "}\n");
        assert(hasError(diags, "redefinition of function 'f'"));
    }

    // Numeric casts (any direction) are legal with no diagnostic.
    {
        const auto diags = analyzeSource(
            "int main() {\n"
            "    float f = 1.5;\n"
            "    int x = (int)f;\n"
            "    char c = (char)x;\n"
            "    float back = (float)c;\n"
            "    return x;\n"
            "}\n");
        assert(diags.empty());
    }

    // A cast to void is an error.
    {
        const auto diags = analyzeSource("int main() {\n"
                                          "    int x = (void)1;\n"
                                          "    return x;\n"
                                          "}\n");
        assert(hasError(diags, "cannot cast 'int' to 'void'"));
    }

    // A cast to/from an aggregate type is an error.
    {
        const auto diags = analyzeSource("struct point { int x; int y; };\n"
                                          "int main() {\n"
                                          "    struct point p;\n"
                                          "    int x = (int)p;\n"
                                          "    return 0;\n"
                                          "}\n");
        assert(hasError(diags, "cannot cast 'struct point' to 'int'"));
    }

    return 0;
}
