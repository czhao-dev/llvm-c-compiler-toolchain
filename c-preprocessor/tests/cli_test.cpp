// Exercises the built c-preprocess binary's CLI argument handling directly
// (the only way to test main.cpp's argument parsing, since it isn't linked
// into pp_core). Runs the binary as a subprocess with stdout/stderr
// redirected to scratch files.

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <sys/wait.h>

#ifndef PP_BINARY
#define PP_BINARY "c-preprocess"
#endif
#ifndef PP_EXAMPLES_DIR
#define PP_EXAMPLES_DIR "examples"
#endif
#ifndef PP_FIXTURES_DIR
#define PP_FIXTURES_DIR "tests/fixtures"
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
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "cpreproc_cli_test";
    std::filesystem::create_directories(dir);
    return dir;
}

RunResult run(const std::vector<std::string> &args) {
    std::filesystem::path outFile = scratchDir() / "stdout.txt";
    std::filesystem::path errFile = scratchDir() / "stderr.txt";

    std::string cmd = shellQuote(PP_BINARY);
    for (const std::string &arg : args) {
        cmd += " " + shellQuote(arg);
    }
    cmd += " >" + shellQuote(outFile.string()) + " 2>" + shellQuote(errFile.string());

    int status = std::system(cmd.c_str());
    int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    return RunResult{exitCode, readFile(outFile), readFile(errFile)};
}

std::string fixture(const std::string &name) { return std::string(PP_FIXTURES_DIR) + "/" + name; }

void expect(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::abort();
    }
}

} // namespace

int main() {
    // No arguments: usage on stderr, exit code 2.
    {
        RunResult r = run({});
        expect(r.exitCode == 2, "no-args exit code should be 2");
        expect(r.err.find("usage:") != std::string::npos, "no-args stderr should show usage");
    }

    // Nonexistent input file: exit 1, clear "cannot open" message.
    {
        RunResult r = run({"/no/such/file.c"});
        expect(r.exitCode == 1, "missing file exit code should be 1");
        expect(r.err.find("cannot open") != std::string::npos,
               "missing file stderr should mention 'cannot open'");
    }

    // -o writes output to a file; no -o writes to stdout.
    {
        std::string input = std::string(PP_EXAMPLES_DIR) + "/main.c";
        std::filesystem::path outPath = scratchDir() / "cli_output.txt";
        RunResult r = run({input, "-o", outPath.string()});
        expect(r.exitCode == 0, "-o run should succeed");
        expect(r.out.empty(), "-o should not also print to stdout");
        std::string written = readFile(outPath);
        expect(written.find("int max = 100;") != std::string::npos,
               "-o output file should contain expanded macro output");

        RunResult r2 = run({input});
        expect(r2.exitCode == 0, "stdout run should succeed");
        expect(r2.out.find("int max = 100;") != std::string::npos,
               "stdout output should contain expanded macro output");
    }

    // -I makes an otherwise-unresolvable include succeed.
    {
        std::string input = fixture("needs_extra_include.c");
        RunResult withoutI = run({input});
        expect(withoutI.exitCode == 1, "missing -I should fail");

        RunResult withI = run({input, "-I", fixture("extra_include")});
        expect(withI.exitCode == 0, "-I should make the include resolvable");
        expect(withI.out.find("flag = 1;") != std::string::npos,
               "-I run should expand FEATURE_ENABLED");
    }

    // An unsupported directive produces a file:line-formatted error on stderr.
    {
        std::filesystem::path bad = scratchDir() / "bad_directive.c";
        std::ofstream(bad) << "#ifdef FOO\n";
        RunResult r = run({bad.string()});
        expect(r.exitCode == 1, "unsupported directive exit code should be 1");
        expect(r.err.find(":1:") != std::string::npos, "error should be attributed to line 1");
        expect(r.err.find("#ifdef") != std::string::npos, "error should name the directive");
    }

    // --help / -h print usage to stdout and exit 0.
    {
        RunResult r = run({"--help"});
        expect(r.exitCode == 0, "--help exit code should be 0");
        expect(r.out.find("usage:") != std::string::npos, "--help should print usage to stdout");

        RunResult r2 = run({"-h"});
        expect(r2.exitCode == 0, "-h exit code should be 0");
    }

    // Unknown option is rejected.
    {
        RunResult r = run({"--bogus", std::string(PP_EXAMPLES_DIR) + "/main.c"});
        expect(r.exitCode == 1, "unknown option exit code should be 1");
        expect(r.err.find("unknown option") != std::string::npos,
               "unknown option message expected");
    }

    // Two positional input files are rejected.
    {
        std::string input = std::string(PP_EXAMPLES_DIR) + "/main.c";
        RunResult r = run({input, input});
        expect(r.exitCode == 1, "multiple positionals exit code should be 1");
        expect(r.err.find("multiple input files") != std::string::npos,
               "multiple positionals message expected");
    }

    std::cout << "cli_test: all checks passed\n";
    return 0;
}
