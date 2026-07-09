#include "snippet.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void expect(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::abort();
    }
}

} // namespace

int main() {
    // caret_lands_under_zero_indexed_column
    {
        std::string result = sa::renderSnippet("    int unused = 5;", 8);
        expect(result == "    int unused = 5;\n        ^", "caret should sit 8 spaces in (0-indexed col 8)");
    }

    // column_zero_has_no_leading_space
    {
        std::string result = sa::renderSnippet("int x;", 0);
        expect(result == "int x;\n^", "column 0 should place the caret at the start of the line");
    }

    // format_with_source_uses_one_indexed_display_column
    {
        sa::Diagnostic d;
        d.path = "example.c";
        d.line = 3;
        d.col = 8;
        d.ruleId = "SA002";
        d.message = "Local variable `unused` is assigned but never used";

        std::string result = sa::formatWithSource(d, "    int unused = 5;");
        expect(result ==
                   "example.c:3:9: SA002 Local variable `unused` is assigned but never used\n"
                   "    int unused = 5;\n"
                   "        ^",
               "formatWithSource should render header (1-indexed col) + source line + caret");
    }

    std::cout << "snippet_test: all checks passed\n";
    return 0;
}
