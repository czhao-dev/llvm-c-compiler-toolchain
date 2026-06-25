#include "magic_number_rule.h"

#include "lexer.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

std::vector<cl::Diagnostic> lint(const std::string &source) {
    auto tokens = cl::Lexer(source, "test.c").tokenize();
    return cl::checkMagicNumberRule(tokens, "test.c");
}

void expect(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::abort();
    }
}

} // namespace

int main() {
    expect(!lint("x == 5;").empty(), "== 5 should be flagged");
    expect(lint("x == 0;").empty(), "== 0 should be exempt");
    expect(lint("x == 1;").empty(), "== 1 should be exempt");
    expect(lint("x != -1;").empty(), "!= -1 should be exempt");
    expect(lint("x >= 1;").empty(), ">= 1 should be exempt");

    // All 6 comparison operators paired with a non-exempt literal are flagged.
    expect(!lint("x == 5;").empty(), "== should be checked");
    expect(!lint("x != 5;").empty(), "!= should be checked");
    expect(!lint("x < 5;").empty(), "< should be checked");
    expect(!lint("x > 5;").empty(), "> should be checked");
    expect(!lint("x <= 5;").empty(), "<= should be checked");
    expect(!lint("x >= 5;").empty(), ">= should be checked");

    // Hex/octal exemption is by parsed value, not string form.
    expect(lint("x == 0x0;").empty(), "hex 0x0 should be exempt (value 0)");
    expect(lint("x == 01;").empty(), "octal 01 should be exempt (C octal value 1)");
    expect(!lint("x == 0x2;").empty(), "hex 0x2 should be flagged");
    expect(!lint("x == 0xFF;").empty(), "hex 0xFF should be flagged");

    // Suffixed literals are flagged correctly.
    expect(!lint("x == 5u;").empty(), "suffixed 5u should be flagged");
    expect(!lint("x == 100L;").empty(), "suffixed 100L should be flagged");

    // Non-adjacent / non-int-literal cases are not flagged.
    expect(lint("x == y;").empty(), "identifier RHS should not be flagged");
    expect(lint("x == 3.14;").empty(), "float literal RHS should not be flagged");
    expect(lint("arr[i] == count;").empty(), "identifier RHS should not be flagged");
    expect(lint("int x = 5;").empty(), "assignment (no comparison) should not be flagged");

    // Negative, non-exempt values are flagged.
    expect(!lint("x != -2;").empty(), "!= -2 should be flagged");

    // Multiple magic numbers in one file are all found, with correct locations.
    {
        auto diags = lint("if (x == 5) {}\nif (y == 7) {}\n");
        expect(diags.size() == 2, "two magic numbers should both be found");
        expect(diags[0].line == 1 && diags[1].line == 2,
               "diagnostics should be attributed to the correct lines");
    }

    std::cout << "magic_number_rule_test: all checks passed\n";
    return 0;
}
