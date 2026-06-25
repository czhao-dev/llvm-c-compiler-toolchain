#include "brace_style_rule.h"

#include "lexer.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

std::vector<cl::Diagnostic> lint(const std::string &source, cl::BraceStyle style) {
    auto tokens = cl::Lexer(source, "test.c").tokenize();
    return cl::checkBraceStyleRule(tokens, "test.c", style);
}

void expect(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::abort();
    }
}

} // namespace

int main() {
    using cl::BraceStyle;

    // K&R: same-line brace is fine, next-line brace is flagged.
    expect(lint("if (x) {\n}\n", BraceStyle::KandR).empty(), "K&R same-line if should not be flagged");
    expect(!lint("if (x)\n{\n}\n", BraceStyle::KandR).empty(), "K&R next-line if should be flagged");
    expect(lint("while (x) {\n}\n", BraceStyle::KandR).empty(),
           "K&R same-line while should not be flagged");
    expect(!lint("while (x)\n{\n}\n", BraceStyle::KandR).empty(),
           "K&R next-line while should be flagged");

    // Allman: reversed.
    expect(!lint("if (x) {\n}\n", BraceStyle::Allman).empty(), "Allman same-line if should be flagged");
    expect(lint("if (x)\n{\n}\n", BraceStyle::Allman).empty(),
           "Allman next-line if should not be flagged");

    // Nested parens in the condition resolve the true closing paren.
    expect(lint("if ((a + b) > (c * d)) {\n}\n", BraceStyle::KandR).empty(),
           "nested parens in condition should not confuse paren matching");
    expect(!lint("if ((a + b) > (c * d))\n{\n}\n", BraceStyle::KandR).empty(),
           "nested parens with next-line brace should still be flagged under K&R");

    // Braceless bodies are out of scope for this rule.
    expect(lint("if (x) foo();\n", BraceStyle::KandR).empty(), "braceless if should not be flagged");

    // do { ... } while (x); -- the trailing clause ends in a semicolon, not
    // a brace, so it's never flagged; this rule only looks forward from
    // if/while for a brace.
    expect(lint("do {\n} while (x);\n", BraceStyle::KandR).empty(),
           "trailing while(x); of a do-while should not be flagged");

    // Truncated input (no closing paren before EOF) doesn't crash and emits nothing.
    expect(lint("if (x", BraceStyle::KandR).empty(), "truncated if condition should not crash or flag");

    // Multiple occurrences are detected independently.
    {
        auto diags = lint("if (x) {\n}\nif (y)\n{\n}\n", BraceStyle::KandR);
        expect(diags.size() == 1, "only the mismatched occurrence should be flagged");
    }

    std::cout << "brace_style_rule_test: all checks passed\n";
    return 0;
}
