#include "rules/sa002_unused_variables.h"

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

    sa::UnusedVariables rule;
    std::vector<sa::Diagnostic> diagnostics = rule.check(tree, source, "example.c", sa::Config{});

    ts_tree_delete(tree);
    ts_parser_delete(parser);
    return diagnostics;
}

} // namespace

int main() {
    // flags_unused_local_variable
    {
        std::vector<sa::Diagnostic> diagnostics = check(
            "int compute(void) {\n    int total = 0;\n    int unused = 42;\n    return total;\n}\n");
        expect(diagnostics.size() == 1, "expected exactly one diagnostic");
        expect(diagnostics[0].message.find("unused") != std::string::npos,
               "message should mention 'unused'");
        expect(diagnostics[0].ruleId == "SA002", "rule id should be SA002");
    }

    // ignores_used_variable
    {
        std::vector<sa::Diagnostic> diagnostics = check(
            "int compute(void) {\n    int total = 0;\n    for (int i = 0; i < 10; i++) {\n        total "
            "+= i;\n    }\n    return total;\n}\n");
        expect(diagnostics.empty(), "a used variable should not be flagged");
    }

    // ignores_underscore_prefixed
    {
        std::vector<sa::Diagnostic> diagnostics =
            check("int compute(void) {\n    int _ignored = expensive_call();\n    return 1;\n}\n");
        expect(diagnostics.empty(), "an underscore-prefixed variable should not be flagged");
    }

    // ignores_global_variable_mutation
    {
        std::vector<sa::Diagnostic> diagnostics =
            check("int counter = 0;\n\nvoid increment(void) {\n    counter = counter + 1;\n}\n");
        expect(diagnostics.empty(), "mutating a global should not be flagged as an unused local");
    }

    // array_size_and_initializer_count_as_use
    {
        std::vector<sa::Diagnostic> diagnostics = check(
            "int compute(int n) {\n    int size = n;\n    int values[size];\n    return values[0];\n}\n");
        expect(diagnostics.empty(), "a variable used as an array size should count as used");
    }

    std::cout << "sa002_unused_variables_test: all checks passed\n";
    return 0;
}
