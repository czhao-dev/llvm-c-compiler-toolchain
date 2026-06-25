// Exercises the built c-lint binary's CLI argument handling directly (the
// only way to test main.cpp's argument parsing, since it isn't linked into
// cl_core). Runs the binary as a subprocess with stdout/stderr redirected
// to scratch files.

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <sys/wait.h>

#ifndef CL_BINARY
#define CL_BINARY "c-lint"
#endif
#ifndef CL_FIXTURES_DIR
#define CL_FIXTURES_DIR "tests/fixtures"
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
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "clinter_cli_test";
    std::filesystem::create_directories(dir);
    return dir;
}

RunResult run(const std::vector<std::string> &args) {
    std::filesystem::path outFile = scratchDir() / "stdout.txt";
    std::filesystem::path errFile = scratchDir() / "stderr.txt";

    std::string cmd = shellQuote(CL_BINARY);
    for (const std::string &arg : args) {
        cmd += " " + shellQuote(arg);
    }
    cmd += " >" + shellQuote(outFile.string()) + " 2>" + shellQuote(errFile.string());

    int status = std::system(cmd.c_str());
    int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

    return RunResult{exitCode, readFile(outFile), readFile(errFile)};
}

std::string fixture(const std::string &name) { return std::string(CL_FIXTURES_DIR) + "/" + name; }

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

    // Clean file: exit 0, empty stdout.
    {
        RunResult r = run({fixture("clean.c")});
        expect(r.exitCode == 0, "clean file exit code should be 0");
        expect(r.out.empty(), "clean file should produce no stdout output");
    }

    // File with issues: exit 1, stdout contains all 5 rule codes.
    {
        RunResult r = run({fixture("kitchen_sink.c")});
        expect(r.exitCode == 1, "file with issues exit code should be 1");
        expect(r.out.find("[CL001]") != std::string::npos, "stdout should contain CL001");
        expect(r.out.find("[CL002]") != std::string::npos, "stdout should contain CL002");
        expect(r.out.find("[CL003]") != std::string::npos, "stdout should contain CL003");
        expect(r.out.find("[CL004]") != std::string::npos, "stdout should contain CL004");
        expect(r.out.find("[CL005]") != std::string::npos, "stdout should contain CL005");
    }

    // Multiple files in one invocation: diagnostics from all files present,
    // correctly attributed by filename.
    {
        RunResult r = run({fixture("kitchen_sink.c"), fixture("allman_style.c")});
        expect(r.exitCode == 1, "multi-file run with issues exit code should be 1");
        expect(r.out.find(fixture("kitchen_sink.c")) != std::string::npos,
               "output should attribute diagnostics to kitchen_sink.c");
        expect(r.out.find(fixture("allman_style.c")) != std::string::npos,
               "output should attribute diagnostics to allman_style.c");
    }

    // Mixed valid + nonexistent file args: stderr mentions the bad one, but
    // the good file's diagnostics still print.
    {
        RunResult r = run({"/no/such/file.c", fixture("kitchen_sink.c")});
        expect(r.exitCode == 1, "mixed valid/missing file exit code should be 1");
        expect(r.err.find("cannot open") != std::string::npos, "stderr should mention the missing file");
        expect(r.out.find("[CL001]") != std::string::npos,
               "the valid file's diagnostics should still be printed");
    }

    // --max-line-length measurably changes what's flagged.
    {
        RunResult withDefault = run({fixture("short_line.c")});
        expect(withDefault.exitCode == 1, "short_line.c should be flagged under the default limit");
        expect(withDefault.out.find("[CL002]") != std::string::npos,
               "default 80-char limit should flag short_line.c");

        RunResult withWideLimit = run({"--max-line-length=100", fixture("short_line.c")});
        expect(withWideLimit.exitCode == 0, "short_line.c should pass with a 100-char limit");
        expect(withWideLimit.out.find("[CL002]") == std::string::npos,
               "100-char limit should not flag short_line.c");
    }

    // --brace-style=allman flips detection in both directions.
    {
        RunResult krRun = run({fixture("allman_style.c")});
        expect(krRun.out.find("[CL005]") != std::string::npos,
               "allman_style.c should be flagged under the default K&R style");

        RunResult allmanRun = run({"--brace-style=allman", fixture("allman_style.c")});
        expect(allmanRun.out.find("[CL005]") == std::string::npos,
               "allman_style.c should not be flagged with --brace-style=allman");
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
        RunResult r = run({"--bogus", fixture("clean.c")});
        expect(r.exitCode == 1, "unknown option exit code should be 1");
        expect(r.err.find("unknown option") != std::string::npos, "unknown option message expected");
    }

    // Malformed --max-line-length values are rejected.
    {
        RunResult r = run({"--max-line-length=notanumber", fixture("clean.c")});
        expect(r.exitCode == 1, "malformed --max-line-length exit code should be 1");
        expect(r.err.find("invalid --max-line-length") != std::string::npos,
               "malformed --max-line-length message expected");

        RunResult r2 = run({"--max-line-length=0", fixture("clean.c")});
        expect(r2.exitCode == 1, "--max-line-length=0 exit code should be 1");
    }

    std::cout << "cli_test: all checks passed\n";
    return 0;
}
