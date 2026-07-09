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
    // caret_lands_under_column
    {
        std::string result = cl::renderSnippet("class Stack {", 7);
        expect(result == "class Stack {\n      ^", "caret should sit under column 7 (0-indexed offset 6)");
    }

    // column_one_has_no_leading_space
    {
        std::string result = cl::renderSnippet("Stack s;", 1);
        expect(result == "Stack s;\n^", "column 1 should place the caret at the start of the line");
    }

    // format_with_source_includes_column_and_snippet
    {
        cl::Diagnostic d;
        d.severity = cl::Severity::Warning;
        d.file = "example.c";
        d.line = 5;
        d.column = 7;
        d.ruleCode = cl::RuleCode::Naming;
        d.message = "identifier 'Stack' should be snake_case";

        std::string result = cl::formatWithSource(d, "class Stack {");
        expect(result ==
                   "example.c:5:7: warning: identifier 'Stack' should be snake_case [CL001]\n"
                   "class Stack {\n"
                   "      ^",
               "formatWithSource should render header + source line + caret");
    }

    std::cout << "snippet_test: all checks passed\n";
    return 0;
}
