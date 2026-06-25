#include "line_rules.h"

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

bool hasRule(const std::vector<cl::Diagnostic> &diagnostics, cl::RuleCode code, int line) {
    for (const auto &d : diagnostics) {
        if (d.ruleCode == code && d.line == line) return true;
    }
    return false;
}

} // namespace

int main() {
    // 80-char line: not flagged. 81-char: flagged. Boundary in both directions.
    {
        std::string line80(80, 'a');
        std::string line81(81, 'a');
        auto diags = cl::checkLineRules(line80 + "\n" + line81 + "\n", "test.c", 80);
        expect(!hasRule(diags, cl::RuleCode::LineLength, 1), "80-char line should not be flagged");
        expect(hasRule(diags, cl::RuleCode::LineLength, 2), "81-char line should be flagged");
    }

    // Custom maxLineLength changes the threshold.
    {
        std::string line50(50, 'a');
        auto tooNarrow = cl::checkLineRules(line50 + "\n", "test.c", 40);
        expect(hasRule(tooNarrow, cl::RuleCode::LineLength, 1), "50-char line should be flagged at max 40");
        auto wideEnough = cl::checkLineRules(line50 + "\n", "test.c", 100);
        expect(!hasRule(wideEnough, cl::RuleCode::LineLength, 1),
               "50-char line should not be flagged at max 100");
    }

    // Trailing space and trailing tab are both flagged; no trailing
    // whitespace is not.
    {
        auto diags = cl::checkLineRules("int x;   \nint y;\t\nint z;\n", "test.c", 80);
        expect(hasRule(diags, cl::RuleCode::TrailingWhitespace, 1), "trailing spaces should be flagged");
        expect(hasRule(diags, cl::RuleCode::TrailingWhitespace, 2), "trailing tab should be flagged");
        expect(!hasRule(diags, cl::RuleCode::TrailingWhitespace, 3),
               "no trailing whitespace should not be flagged");
    }

    // A line that is both too long and has trailing whitespace emits both codes.
    {
        std::string longLine(85, 'a');
        longLine += "  ";
        auto diags = cl::checkLineRules(longLine + "\n", "test.c", 80);
        expect(hasRule(diags, cl::RuleCode::LineLength, 1), "long+trailing-ws line should flag length");
        expect(hasRule(diags, cl::RuleCode::TrailingWhitespace, 1),
               "long+trailing-ws line should flag trailing whitespace");
    }

    // CRLF line endings don't cause false positives.
    {
        auto diags = cl::checkLineRules("int x;\r\nint y;\r\n", "test.c", 80);
        expect(diags.empty(), "CRLF-terminated short lines should not be flagged");
    }

    // The last line, even without a trailing newline, is still checked.
    {
        std::string longLine(90, 'a');
        auto diags = cl::checkLineRules("int x;\n" + longLine, "test.c", 80);
        expect(hasRule(diags, cl::RuleCode::LineLength, 2),
               "final line without a trailing newline should still be checked");
    }

    std::cout << "line_rules_test: all checks passed\n";
    return 0;
}
