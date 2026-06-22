#include "preprocessor.h"

#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifndef PP_EXAMPLES_DIR
#define PP_EXAMPLES_DIR "examples"
#endif

namespace {

std::string readFile(const std::string &path) {
    std::ifstream file(path, std::ios::binary);
    assert(file && "could not open example file");
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string trimLeft(const std::string &s) {
    std::size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r')) i += 1;
    return s.substr(i);
}

std::vector<std::string> splitLines(const std::string &text) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == '\n') {
            lines.push_back(text.substr(start, i - start));
            start = i + 1;
        }
    }
    return lines;
}

} // namespace

int main() {
    pp::Preprocessor preprocessor;
    std::string output = preprocessor.preprocessFile(std::string(PP_EXAMPLES_DIR) + "/main.c");

    // Every directive was consumed: no output line begins with '#'.
    for (const std::string &line : splitLines(output)) {
        std::string trimmed = trimLeft(line);
        assert((trimmed.empty() || trimmed[0] != '#') && "a directive leaked into the output");
    }

    // Macro usages resolved to their literal values.
    assert(output.find("int max = 100;") != std::string::npos);
    assert(output.find("int min = 0;") != std::string::npos);
    assert(output.find("int circumference = (3 * 2);") != std::string::npos);
    assert(output.find("printf(\"Welcome!\");") != std::string::npos);
    assert(output.find("printf(\"%s\\n\", \"units\");") != std::string::npos);

    // A string literal containing "//" survives completely unmolested.
    assert(output.find("\"See http://example.com for details\"") != std::string::npos);

    // Line-number preservation end to end. Known structure of examples/ (see
    // the files themselves): main.c's line 1 (a // comment) becomes 1 blank
    // output line; line 2 (#include "constants.h", four #define lines)
    // becomes 4 blank output lines; line 3 (#include "geometry.h": one
    // nested #include plus two #define lines) becomes 3; line 4
    // (#include "util.h": a 5-line block comment plus two directive lines)
    // becomes 7; lines 5-8 (a blank line plus a 3-line block comment) become
    // 4 more blank lines. So "int main(void) {" (main.c's line 9) must land
    // on output line 1+4+3+7+4+1 = 20.
    {
        int expectedLine = 1 + 4 + 3 + 7 + 4 + 1;
        std::size_t pos = output.find("int main(void) {");
        assert(pos != std::string::npos);
        int actualLine = static_cast<int>(std::count(output.begin(), output.begin() + pos, '\n')) + 1;
        if (actualLine != expectedLine) {
            std::cerr << "expected \"int main(void) {\" on output line " << expectedLine
                      << ", found it on line " << actualLine << "\n";
            std::abort();
        }
    }

    // Golden-output comparison, hand-verified once and checked in.
    {
        std::string expected = readFile(std::string(PP_EXAMPLES_DIR) + "/main.expected.txt");
        if (output != expected) {
            std::cerr << "output does not match examples/main.expected.txt\n--- actual ---\n"
                      << output << "\n--- expected ---\n" << expected << "\n";
            std::abort();
        }
    }

    std::cout << "preprocessor_test: all checks passed\n";
    return 0;
}
