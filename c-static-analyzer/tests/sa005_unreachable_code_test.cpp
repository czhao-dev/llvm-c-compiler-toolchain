#include "rules/sa005_unreachable_code.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <tree_sitter/api.h>
#include <tree_sitter/tree-sitter-c.h>

namespace {

void expect(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::abort();
    }
}

std::vector<sa::Diagnostic> check(const std::string &source) {
    TSParser *parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_c());
    TSTree *tree = ts_parser_parse_string(parser, nullptr, source.c_str(), static_cast<uint32_t>(source.size()));

    sa::UnreachableCode rule;
    std::vector<sa::Diagnostic> diagnostics = rule.check(tree, source, "example.c", sa::Config{});

    ts_tree_delete(tree);
    ts_parser_delete(parser);
    return diagnostics;
}

} // namespace

int main() {
    // flags_code_after_return
    {
        std::vector<sa::Diagnostic> diagnostics =
            check("int f(void) {\n    return 1;\n    printf(\"never runs\");\n}\n");
        expect(diagnostics.size() == 1, "expected exactly one diagnostic");
        expect(diagnostics[0].ruleId == "SA005", "rule id should be SA005");
        expect(diagnostics[0].line == 3, "unreachable line should be 3");
    }

    // flags_code_after_break_in_loop
    {
        std::vector<sa::Diagnostic> diagnostics = check(
            "void f(void) {\n    for (int i = 0; i < 10; i++) {\n        break;\n        "
            "printf(\"%d\", i);\n    }\n}\n");
        // The CFG-based rewrite correctly flags two things here, not one:
        // the printf (textually after the break) and the loop's own `i++`
        // update clause -- since the body unconditionally breaks on its
        // first statement, the update clause can never actually run
        // either. The old textual-adjacency heuristic could only see the
        // former (the update clause isn't textually adjacent to `break` in
        // the same block), so this is a deliberate behavior improvement,
        // not a regression.
        expect(diagnostics.size() == 2, "code after break and the now-unreachable loop update should be flagged");
    }

    // flags_code_after_return_in_case
    {
        std::vector<sa::Diagnostic> diagnostics = check(
            "int f(int x) {\n    switch (x) {\n        case 1:\n            return 1;\n            "
            "return 2;\n    }\n    return 0;\n}\n");
        expect(diagnostics.size() == 1, "code after return inside a case should be flagged");
    }

    // ignores_reachable_code
    {
        std::vector<sa::Diagnostic> diagnostics =
            check("int f(int x) {\n    if (x) {\n        return 1;\n    }\n    return 2;\n}\n");
        expect(diagnostics.empty(), "reachable code should not be flagged");
    }

    std::cout << "sa005_unreachable_code_test: all checks passed\n";
    return 0;
}
