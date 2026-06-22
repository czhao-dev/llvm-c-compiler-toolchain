#include "comment_stripper.h"

#include <cassert>
#include <algorithm>
#include <iostream>
#include <string>

#include "diagnostics.h"

namespace {

void expectEqual(const std::string &source, const std::string &expected) {
    std::string actual = pp::stripComments(source, "<test>");
    if (actual != expected) {
        std::cerr << "stripComments(\"" << source << "\")\n  expected: \"" << expected
                   << "\"\n  actual:   \"" << actual << "\"\n";
        std::abort();
    }
}

long newlineCount(const std::string &s) { return std::count(s.begin(), s.end(), '\n'); }

void expectThrows(const std::string &source, const std::string &messageSubstring) {
    try {
        pp::stripComments(source, "<test>");
    } catch (const pp::PreprocessorError &ex) {
        std::string what = ex.what();
        if (what.find(messageSubstring) == std::string::npos) {
            std::cerr << "expected error containing \"" << messageSubstring << "\", got \""
                      << what << "\"\n";
            std::abort();
        }
        return;
    }
    std::cerr << "expected stripComments(\"" << source << "\") to throw\n";
    std::abort();
}

} // namespace

int main() {
    // Line comments.
    expectEqual("int x = 1; // set x\nint y = 2;", "int x = 1; \nint y = 2;");

    // Single-line block comment.
    expectEqual("int x /* inline */ = 1;", "int x  = 1;");

    // Multi-line block comment preserves the newline count.
    {
        std::string source = "int x = 1;\n/* line one\n   line two\n   line three */\nint y = 2;";
        std::string result = pp::stripComments(source, "<test>");
        assert(newlineCount(result) == newlineCount(source));
        assert(result == "int x = 1;\n\n\n\nint y = 2;");
    }

    // Unterminated block comment errors, attributed to the line it started on.
    {
        std::string source = "int x = 1;\n/* never closed\nint y = 2;";
        try {
            pp::stripComments(source, "<test>");
            std::cerr << "expected unterminated block comment to throw\n";
            std::abort();
        } catch (const pp::PreprocessorError &ex) {
            assert(ex.line() == 2);
            std::string what = ex.what();
            assert(what.find("unterminated") != std::string::npos);
        }
    }

    // // inside a string literal is not a comment.
    expectEqual("const char *url = \"http://example.com\";",
                "const char *url = \"http://example.com\";");

    // /* */-looking text inside a string literal is not a comment.
    expectEqual("const char *s = \"a/*b*/c\";", "const char *s = \"a/*b*/c\";");

    // Escaped quote inside a string doesn't terminate the literal early.
    expectEqual("const char *s = \"she said \\\"hi\\\" // not a comment\";",
                "const char *s = \"she said \\\"hi\\\" // not a comment\";");

    // Unterminated string / char literals error.
    expectThrows("const char *s = \"never closed", "unterminated");
    expectThrows("char c = 'x", "unterminated");

    // // has no special meaning inside a /* */ block comment.
    expectEqual("/* outer // still a comment */ int x;", " int x;");

    // /* */ has no special meaning inside a // line comment (whole line is dropped anyway).
    expectEqual("// looks like /* a block start\nint x;", "\nint x;");

    // Real operators (* and /) pass through unchanged when not part of a comment marker.
    expectEqual("int r = a * b / c;", "int r = a * b / c;");

    // Passthrough with no comments at all.
    expectEqual("int main() { return 0; }", "int main() { return 0; }");
    expectEqual("", "");

    std::cout << "comment_stripper_test: all checks passed\n";
    return 0;
}
