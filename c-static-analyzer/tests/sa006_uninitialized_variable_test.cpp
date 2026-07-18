#include "rules/sa006_uninitialized_variable.h"

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

    sa::UninitializedVariable rule;
    std::vector<sa::Diagnostic> diagnostics = rule.check(tree, source, "example.c", sa::Config{});

    ts_tree_delete(tree);
    ts_parser_delete(parser);
    return diagnostics;
}

} // namespace

int main() {
    // flags_read_before_assignment
    {
        std::vector<sa::Diagnostic> diagnostics = check("int compute(void) {\n    int x;\n    return x;\n}\n");
        expect(diagnostics.size() == 1, "expected exactly one diagnostic");
        expect(diagnostics[0].message.find("x") != std::string::npos, "message should mention 'x'");
        expect(diagnostics[0].ruleId == "SA006", "rule id should be SA006");
    }

    // ignores_assigned_before_read
    {
        std::vector<sa::Diagnostic> diagnostics =
            check("int compute(void) {\n    int x;\n    x = 5;\n    return x;\n}\n");
        expect(diagnostics.empty(), "a variable assigned before its first read should not be flagged");
    }

    // ignores_declared_with_initializer
    {
        std::vector<sa::Diagnostic> diagnostics = check("int compute(void) {\n    int x = 5;\n    return x;\n}\n");
        expect(diagnostics.empty(), "a variable declared with an initializer should not be flagged");
    }

    // ignores_underscore_prefixed
    {
        std::vector<sa::Diagnostic> diagnostics =
            check("int compute(void) {\n    int _tmp;\n    return _tmp;\n}\n");
        expect(diagnostics.empty(), "an underscore-prefixed variable should not be flagged");
    }

    // ignores_array_declarations
    {
        std::vector<sa::Diagnostic> diagnostics =
            check("int compute(void) {\n    int arr[3];\n    return arr[0];\n}\n");
        expect(diagnostics.empty(), "array-typed locals are not analyzed by this rule");
    }

    // ignores_never_referenced_again
    {
        std::vector<sa::Diagnostic> diagnostics = check("int compute(void) {\n    int unused;\n    return 0;\n}\n");
        expect(diagnostics.empty(), "a variable never referenced again is SA002's job, not SA006's");
    }

    // ignores_field_by_field_struct_initialization
    {
        std::vector<sa::Diagnostic> diagnostics = check(
            "struct point { int x; int y; };\n"
            "int compute(void) {\n"
            "    struct point pt;\n"
            "    pt.x = 1;\n"
            "    pt.y = 2;\n"
            "    return pt.x;\n"
            "}\n");
        expect(diagnostics.empty(), "writing every field before the first read should not be flagged");
    }

    // flags_struct_field_read_before_any_write
    {
        std::vector<sa::Diagnostic> diagnostics = check(
            "struct point { int x; int y; };\n"
            "int compute(void) {\n"
            "    struct point pt;\n"
            "    return pt.x;\n"
            "}\n");
        expect(diagnostics.size() == 1, "a field read before any field write should be flagged");
        expect(diagnostics[0].message.find("pt") != std::string::npos, "message should mention 'pt'");
    }

    // flags_uninitialized_on_some_but_not_all_paths
    //
    // The old textual-pass implementation missed this: the first *textual*
    // reference to `x` after its declaration is the write inside the `if`,
    // so it never flagged this even though `x` is genuinely uninitialized
    // whenever `c` is false. The CFG-based dataflow explores both branches
    // and correctly finds the bad read on the false path.
    {
        std::vector<sa::Diagnostic> diagnostics =
            check("int compute(int c) {\n    int x;\n    if (c) {\n        x = 1;\n    }\n    return x;\n}\n");
        expect(diagnostics.size() == 1, "a variable uninitialized on some paths should be flagged");
    }

    // flags_read_only_reachable_when_condition_true
    //
    // The old implementation flagged this too, but only because the first
    // textual reference after declaration happened to be the read inside
    // the `if` -- coincidentally correct, not real branch analysis. The
    // dataflow rewrite flags it for the right reason: the if-branch is a
    // real path from the declaration to this read with no write on it.
    {
        std::vector<sa::Diagnostic> diagnostics = check(
            "int compute(int c) {\n    int x;\n    if (c) {\n        printf(\"%d\", x);\n    } else {\n     "
            "   x = 1;\n    }\n    return x;\n}\n");
        expect(diagnostics.size() == 1, "a read reachable without an intervening write should be flagged");
    }

    // ignores_write_through_pointer_address
    //
    // A known, documented limitation carried over unchanged from the old
    // implementation: `&x` is classified as a read (it isn't the left-hand
    // side of `=`), so passing an uninitialized local by address to be
    // filled in (e.g. by scanf) is still misclassified. This is a write-
    // classification gap, not a control-flow one, so the CFG rewrite alone
    // doesn't fix it -- see docs/SPEC.md.
    {
        std::vector<sa::Diagnostic> diagnostics =
            check("int compute(void) {\n    int x;\n    scanf(\"%d\", &x);\n    return x;\n}\n");
        expect(diagnostics.size() == 1, "write-through-pointer is still not recognized as initialization");
    }

    std::cout << "sa006_uninitialized_variable_test: all checks passed\n";
    return 0;
}
