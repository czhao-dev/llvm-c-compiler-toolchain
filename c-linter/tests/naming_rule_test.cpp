#include "naming_rule.h"

#include "lexer.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

std::vector<cl::Diagnostic> lintNames(const std::string &source) {
    auto tokens = cl::Lexer(source, "test.c").tokenize();
    return cl::checkNamingRule(tokens, "test.c");
}

void expect(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::abort();
    }
}

} // namespace

int main() {
    expect(!lintNames("int totalCount;").empty(), "camelCase identifier should be flagged");
    expect(lintNames("int total_count;").empty(), "snake_case identifier should not be flagged");
    expect(lintNames("int MAX_SIZE;").empty(), "ALL_CAPS with underscore should not be flagged");
    expect(!lintNames("int PI;").empty(),
           "single-word ALLCAPS with no underscore should be flagged, per the literal rule");
    expect(!lintNames("int TotalCount;").empty(), "PascalCase should be flagged");

    // Real C keywords lexed as bare identifiers are always lowercase and
    // never flagged.
    {
        auto diags = lintNames("return int struct sizeof x;");
        expect(diags.empty(), "keywords and a lowercase identifier should produce no diagnostics");
    }

    std::cout << "naming_rule_test: all checks passed\n";
    return 0;
}
