// Frozen output of `c-static-analyzer scan examples/sample_issues.c
// --no-config`, originally captured from the retired Python/Rust
// implementations to verify byte-for-byte parity across the C++ port.
// Kept as a regression fixture for the reference example file.

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <sys/wait.h>

#ifndef SA_BINARY
#define SA_BINARY "c-static-analyzer"
#endif
#ifndef SA_SOURCE_DIR
#define SA_SOURCE_DIR "."
#endif

namespace {

struct RunResult {
    int exitCode;
    std::string out;
    std::string err;
};

std::string shellQuote(const std::string &s) {
    std::string quoted = "'";
    for (char c : s) {
        if (c == '\'') {
            quoted += "'\"'\"'";
        } else {
            quoted += c;
        }
    }
    quoted += "'";
    return quoted;
}

std::string readFile(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return "";
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::filesystem::path scratchDir() {
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "sa_golden_test";
    std::filesystem::create_directories(dir);
    return dir;
}

RunResult run(const std::vector<std::string> &args) {
    std::filesystem::path outFile = scratchDir() / "stdout.txt";
    std::filesystem::path errFile = scratchDir() / "stderr.txt";

    std::string cmd = "cd " + shellQuote(SA_SOURCE_DIR) + " && " + shellQuote(SA_BINARY);
    for (const std::string &arg : args) cmd += " " + shellQuote(arg);
    cmd += " >" + shellQuote(outFile.string()) + " 2>" + shellQuote(errFile.string());

    int status = std::system(cmd.c_str());
    int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return RunResult{exitCode, readFile(outFile), readFile(errFile)};
}

void expect(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::abort();
    }
}

} // namespace

int main() {
    const std::string expectedStdout =
        "examples/sample_issues.c:3: SA001 Function `complex_calc` has cyclomatic complexity 12 "
        "(threshold 10)\n"
        "examples/sample_issues.c:18: SA004 Function `classify` may not return a value on all code "
        "paths\n"
        "examples/sample_issues.c:31: SA003 Control flow nested 5 levels deep (threshold 4)\n"
        "examples/sample_issues.c:41: SA002 Local variable `unused` is assigned but never used\n"
        "examples/sample_issues.c:45: SA005 Unreachable code after `return`\n";
    const std::string expectedStderr = "\n5 issue(s) found.\n";

    RunResult result = run({"scan", "examples/sample_issues.c", "--no-config"});

    expect(result.exitCode == 1, "expected exit code 1, got " + std::to_string(result.exitCode));
    expect(result.out == expectedStdout, "stdout did not match the golden fixture:\n" + result.out);
    expect(result.err == expectedStderr, "stderr did not match the golden fixture:\n" + result.err);

    std::cout << "golden_test: all checks passed\n";
    return 0;
}
