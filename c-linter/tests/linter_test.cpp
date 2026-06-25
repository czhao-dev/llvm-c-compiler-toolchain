#include "linter.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifndef CL_FIXTURES_DIR
#define CL_FIXTURES_DIR "tests/fixtures"
#endif
#ifndef CL_EXAMPLES_DIR
#define CL_EXAMPLES_DIR "examples"
#endif

namespace {

void expect(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::abort();
    }
}

std::string readFile(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    expect(static_cast<bool>(in), "could not open fixture: " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool hasCode(const std::vector<cl::Diagnostic> &diags, cl::RuleCode code) {
    for (const auto &d : diags) {
        if (d.ruleCode == code) return true;
    }
    return false;
}

} // namespace

int main() {
    cl::Linter linter;

    // kitchen_sink.c hits all 5 rule codes at known lines, correctly
    // interleaved by line rather than grouped by rule -- this is the
    // direct test of the aggregation/sort contract.
    {
        std::string path = std::string(CL_FIXTURES_DIR) + "/kitchen_sink.c";
        auto diags = linter.lintSource(readFile(path), path);

        expect(diags.size() == 7, "kitchen_sink.c should produce exactly 7 diagnostics");
        expect(hasCode(diags, cl::RuleCode::Naming), "CL001 should be present");
        expect(hasCode(diags, cl::RuleCode::LineLength), "CL002 should be present");
        expect(hasCode(diags, cl::RuleCode::TrailingWhitespace), "CL003 should be present");
        expect(hasCode(diags, cl::RuleCode::MagicNumber), "CL004 should be present");
        expect(hasCode(diags, cl::RuleCode::BraceStyle), "CL005 should be present");

        std::vector<int> expectedLines = {1, 2, 3, 4, 4, 5, 6};
        for (std::size_t i = 0; i < expectedLines.size(); ++i) {
            expect(diags[i].line == expectedLines[i],
                   "diagnostic " + std::to_string(i) + " should be on line " +
                       std::to_string(expectedLines[i]));
        }
        expect(diags[0].ruleCode == cl::RuleCode::Naming, "line-1 diagnostic should be CL001");
        expect(diags[3].ruleCode == cl::RuleCode::Naming, "line-4 first diagnostic should be CL001");
        expect(diags[4].ruleCode == cl::RuleCode::MagicNumber,
               "line-4 second diagnostic should be CL004");
    }

    // clean.c produces zero diagnostics.
    {
        std::string path = std::string(CL_FIXTURES_DIR) + "/clean.c";
        auto diags = linter.lintSource(readFile(path), path);
        expect(diags.empty(), "clean.c should produce zero diagnostics");
    }

    // LinterOptions overrides change the result set on the same input.
    {
        std::string path = std::string(CL_FIXTURES_DIR) + "/allman_style.c";
        std::string source = readFile(path);

        cl::Linter defaultLinter; // K&R default
        expect(hasCode(defaultLinter.lintSource(source, path), cl::RuleCode::BraceStyle),
               "allman_style.c should be flagged under the default K&R style");

        cl::LinterOptions allmanOptions;
        allmanOptions.braceStyle = cl::BraceStyle::Allman;
        cl::Linter allmanLinter(allmanOptions);
        expect(!hasCode(allmanLinter.lintSource(source, path), cl::RuleCode::BraceStyle),
               "allman_style.c should not be flagged when configured for Allman style");
    }
    {
        std::string path = std::string(CL_FIXTURES_DIR) + "/short_line.c";
        std::string source = readFile(path);

        cl::Linter defaultLinter; // max 80
        expect(hasCode(defaultLinter.lintSource(source, path), cl::RuleCode::LineLength),
               "short_line.c should be flagged under the default 80-char limit");

        cl::LinterOptions wideOptions;
        wideOptions.maxLineLength = 100;
        cl::Linter wideLinter(wideOptions);
        expect(!hasCode(wideLinter.lintSource(source, path), cl::RuleCode::LineLength),
               "short_line.c should not be flagged with a 100-char limit");
    }

    // examples/sample.c: structural assertions (not byte-for-byte), since
    // pinning exact diagnostic text/count would be fragile if wording changes.
    {
        std::string path = std::string(CL_EXAMPLES_DIR) + "/sample.c";
        auto diags = linter.lintSource(readFile(path), path);
        expect(hasCode(diags, cl::RuleCode::Naming), "sample.c should flag naming (topIndex/myStack)");
        expect(hasCode(diags, cl::RuleCode::MagicNumber), "sample.c should flag the magic number 31");
        expect(hasCode(diags, cl::RuleCode::BraceStyle), "sample.c should flag the Allman-style if brace");
        expect(!hasCode(diags, cl::RuleCode::LineLength), "sample.c has no overlong lines");
        expect(!hasCode(diags, cl::RuleCode::TrailingWhitespace), "sample.c has no trailing whitespace");
    }

    std::cout << "linter_test: all checks passed\n";
    return 0;
}
