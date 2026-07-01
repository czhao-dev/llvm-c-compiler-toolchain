#include "rules/sa004_missing_return.h"

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

    sa::MissingReturn rule;
    std::vector<sa::Diagnostic> diagnostics = rule.check(tree, source, "example.c", sa::Config{});

    ts_tree_delete(tree);
    ts_parser_delete(parser);
    return diagnostics;
}

} // namespace

int main() {
    // flags_missing_else_branch
    {
        std::vector<sa::Diagnostic> diagnostics = check(
            "const char *classify(int x) {\n    if (x > 0) {\n        return \"positive\";\n    }\n}\n");
        expect(diagnostics.size() == 1, "expected exactly one diagnostic");
        expect(diagnostics[0].ruleId == "SA004", "rule id should be SA004");
    }

    // ignores_complete_if_else
    {
        std::vector<sa::Diagnostic> diagnostics = check(
            "const char *classify(int x) {\n    if (x > 0) {\n        return \"positive\";\n    } else "
            "{\n        return \"non-positive\";\n    }\n}\n");
        expect(diagnostics.empty(), "an exhaustive if/else should not be flagged");
    }

    // ignores_void_function
    {
        std::vector<sa::Diagnostic> diagnostics =
            check("void log_message(const char *message) {\n    printf(\"%s\", message);\n}\n");
        expect(diagnostics.empty(), "a void function should never be flagged");
    }

    // ignores_exhaustive_if_elif_else_chain
    {
        std::vector<sa::Diagnostic> diagnostics = check(
            "int parse_value(const char *value) {\n    if (value == 0) {\n        return -1;\n    } "
            "else if (*value == '\\0') {\n        return 0;\n    } else {\n        return "
            "atoi(value);\n    }\n}\n");
        expect(diagnostics.empty(), "an exhaustive if/elif/else chain should not be flagged");
    }

    // ignores_infinite_while_loop_without_break
    {
        std::vector<sa::Diagnostic> diagnostics = check(
            "const char *serve(void) {\n    while (1) {\n        if (should_stop()) {\n            "
            "return \"done\";\n        }\n        handle();\n    }\n}\n");
        expect(diagnostics.empty(), "an infinite while loop without break should not be flagged");
    }

    // ignores_infinite_do_while_loop_without_break
    {
        std::vector<sa::Diagnostic> diagnostics = check(
            "int serve_loop(void) {\n    do {\n        if (should_stop()) {\n            return 1;\n    "
            "    }\n    } while (1);\n}\n");
        expect(diagnostics.empty(), "an infinite do-while loop without break should not be flagged");
    }

    // flags_while_loop_with_break
    {
        std::vector<sa::Diagnostic> diagnostics = check(
            "int find_first(int n) {\n    while (1) {\n        if (n > 0) {\n            break;\n       "
            " }\n    }\n}\n");
        expect(diagnostics.size() == 1, "an infinite loop with a break should be flagged");
    }

    std::cout << "sa004_missing_return_test: all checks passed\n";
    return 0;
}
