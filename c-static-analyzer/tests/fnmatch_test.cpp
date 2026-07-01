#include "fnmatch.h"

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
    // star_matches_any_suffix
    expect(sa::fnmatch("tests/test_foo.py", "tests/*"), "tests/* should match tests/test_foo.py");
    expect(sa::fnmatch("foo.py", "*.py"), "*.py should match foo.py");
    expect(!sa::fnmatch("foo.txt", "*.py"), "*.py should not match foo.txt");

    // question_mark_matches_single_char
    expect(sa::fnmatch("a.py", "?.py"), "?.py should match a.py");
    expect(!sa::fnmatch("ab.py", "?.py"), "?.py should not match ab.py");

    // bracket_set_matches_listed_chars
    expect(sa::fnmatch("a.py", "[abc].py"), "[abc].py should match a.py");
    expect(!sa::fnmatch("d.py", "[abc].py"), "[abc].py should not match d.py");
    expect(sa::fnmatch("d.py", "[!abc].py"), "[!abc].py should match d.py");

    // bracket_range_matches
    expect(sa::fnmatch("m.py", "[a-z].py"), "[a-z].py should match m.py");
    expect(!sa::fnmatch("M.py", "[a-z].py"), "[a-z].py should not match M.py (case-sensitive)");

    std::cout << "fnmatch_test: all checks passed\n";
    return 0;
}
