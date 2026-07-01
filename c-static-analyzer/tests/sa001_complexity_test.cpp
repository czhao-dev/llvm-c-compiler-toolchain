#include "rules/sa001_complexity.h"

#include <cstdlib>
#include <iostream>
#include <set>
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

    sa::Complexity rule;
    std::vector<sa::Diagnostic> diagnostics = rule.check(tree, source, "example.c", config);

    ts_tree_delete(tree);
    ts_parser_delete(parser);
    return diagnostics;
}

// Extracts the backtick-quoted function name out of a diagnostic message,
// e.g. "Function `inner` has ..." -> "inner".
std::string extractName(const std::string &message) {
    std::size_t first = message.find('`');
    std::size_t second = message.find('`', first + 1);
    return message.substr(first + 1, second - first - 1);
}

} // namespace

int main() {
    // simple_function_is_not_flagged
    {
        std::vector<sa::Diagnostic> diagnostics =
            check("int add(int a, int b) {\n    return a + b;\n}\n", sa::Config{});
        expect(diagnostics.empty(), "a trivial function should not be flagged");
    }

    // flags_high_complexity_function
    {
        sa::Config config;
        config.maxComplexity = 3;
        std::string source =
            "const char *classify(int x) {\n    if (x > 0) {\n        if (x > 10) {\n            return "
            "\"big\";\n        }\n        return \"small\";\n    } else if (x < 0) {\n        return "
            "\"negative\";\n    }\n    return \"zero\";\n}\n";
        std::vector<sa::Diagnostic> diagnostics = check(source, config);
        expect(diagnostics.size() == 1, "expected exactly one diagnostic");
        expect(diagnostics[0].ruleId == "SA001", "rule id should be SA001");
    }

    // multiple_functions_scored_independently
    {
        sa::Config config;
        config.maxComplexity = 1;
        std::string source =
            "int outer(void) {\n    return 1;\n}\n\nint inner(int x) {\n    if (x) {\n        return "
            "1;\n    }\n    return 2;\n}\n";
        std::vector<sa::Diagnostic> diagnostics = check(source, config);
        std::set<std::string> names;
        for (const auto &d : diagnostics) names.insert(extractName(d.message));
        expect((names == std::set<std::string>{"inner"}), "only 'inner' should be flagged");
    }

    std::cout << "sa001_complexity_test: all checks passed\n";
    return 0;
}
