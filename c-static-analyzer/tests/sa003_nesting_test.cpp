#include "rules/sa003_nesting.h"

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

std::vector<sa::Diagnostic> check(const std::string &source, const sa::Config &config) {
    TSParser *parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_c());
    TSTree *tree = ts_parser_parse_string(parser, nullptr, source.c_str(), static_cast<uint32_t>(source.size()));

    sa::Nesting rule;
    std::vector<sa::Diagnostic> diagnostics = rule.check(tree, source, "example.c", config);

    ts_tree_delete(tree);
    ts_parser_delete(parser);
    return diagnostics;
}

} // namespace

int main() {
    // shallow_nesting_is_not_flagged
    {
        std::vector<sa::Diagnostic> diagnostics =
            check("int f(int x) {\n    if (x) {\n        return 1;\n    }\n    return 0;\n}\n", sa::Config{});
        expect(diagnostics.empty(), "shallow nesting should not be flagged");
    }

    // flags_deep_nesting
    {
        sa::Config config;
        config.maxNesting = 2;
        std::string source =
            "int f(int x) {\n    if (x) {\n        for (int i = 0; i < x; i++) {\n            while (i "
            "> 0) {\n                i--;\n            }\n        }\n    }\n    return 0;\n}\n";
        std::vector<sa::Diagnostic> diagnostics = check(source, config);
        expect(diagnostics.size() == 1, "expected exactly one diagnostic");
        expect(diagnostics[0].ruleId == "SA003", "rule id should be SA003");
    }

    // elif_chain_does_not_count_as_nesting
    {
        sa::Config config;
        config.maxNesting = 1;
        std::string source =
            "const char *f(int x) {\n    if (x == 1) {\n        return \"one\";\n    } else if (x == 2) "
            "{\n        return \"two\";\n    } else if (x == 3) {\n        return \"three\";\n    } else "
            "{\n        return \"other\";\n    }\n}\n";
        std::vector<sa::Diagnostic> diagnostics = check(source, config);
        expect(diagnostics.empty(), "an elif chain should not count as nesting");
    }

    // else_with_nested_if_does_count
    {
        sa::Config config;
        config.maxNesting = 1;
        std::string source =
            "int f(int x, int y) {\n    if (x) {\n        return 1;\n    } else {\n        if (y) {\n    "
            "        return 2;\n        }\n    }\n}\n";
        std::vector<sa::Diagnostic> diagnostics = check(source, config);
        expect(diagnostics.size() == 1, "a nested if inside a real else should count as nesting");
    }

    // reports_only_once_per_function
    {
        sa::Config config;
        config.maxNesting = 1;
        std::string source =
            "int f(int x) {\n    if (x) {\n        if (x) {\n            if (x) {\n                "
            "return 1;\n            }\n        }\n    }\n    return 0;\n}\n";
        std::vector<sa::Diagnostic> diagnostics = check(source, config);
        expect(diagnostics.size() == 1, "only the first violation per function should be reported");
    }

    // switch_case_counts_as_nesting
    {
        sa::Config config;
        config.maxNesting = 1;
        std::string source =
            "int f(int x) {\n    switch (x) {\n        case 1:\n            if (x) {\n                "
            "return 1;\n            }\n            return 0;\n    }\n    return 0;\n}\n";
        std::vector<sa::Diagnostic> diagnostics = check(source, config);
        expect(diagnostics.size() == 1, "a case body counts as nesting");
    }

    std::cout << "sa003_nesting_test: all checks passed\n";
    return 0;
}
