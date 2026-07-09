// Exercises the built c-static-analyzer binary's CLI argument handling
// directly (the only way to test main.cpp's argument parsing, since it
// isn't linked into sa_core). Runs the binary as a subprocess with
// stdout/stderr redirected to scratch files.

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#ifndef SA_BINARY
#define SA_BINARY "c-static-analyzer"
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
    std::filesystem::path dir =
        std::filesystem::temp_directory_path() / ("sa_cli_test_" + std::to_string(getpid()));
    std::filesystem::create_directories(dir);
    return dir;
}

RunResult run(const std::vector<std::string> &args) {
    std::filesystem::path dir = scratchDir();
    std::filesystem::path outFile = dir / "stdout.txt";
    std::filesystem::path errFile = dir / "stderr.txt";

    std::string cmd = shellQuote(SA_BINARY);
    for (const std::string &arg : args) cmd += " " + shellQuote(arg);
    cmd += " >" + shellQuote(outFile.string()) + " 2>" + shellQuote(errFile.string());

    int status = std::system(cmd.c_str());
    int exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return RunResult{exitCode, readFile(outFile), readFile(errFile)};
}

void writeFile(const std::filesystem::path &path, const std::string &contents) {
    std::ofstream out(path, std::ios::binary);
    out << contents;
}

void expect(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::abort();
    }
}

} // namespace

int main() {
    // scan_clean_file_exits_zero
    {
        std::filesystem::path dir = scratchDir();
        std::filesystem::path file = dir / "clean.c";
        writeFile(file, "int add(int a, int b) {\n    return a + b;\n}\n");

        RunResult r = run({"scan", file.string(), "--no-config"});
        expect(r.exitCode == 0, "clean file should exit 0");
        expect(r.out.empty(), "clean file should produce no stdout");
    }

    // scan_file_with_issues_exits_one
    {
        std::filesystem::path dir = scratchDir();
        std::filesystem::path file = dir / "bad.c";
        writeFile(file, "int classify(int x) {\n    if (x > 0) {\n        return 1;\n    }\n}\n");

        RunResult r = run({"scan", file.string(), "--no-config"});
        expect(r.exitCode == 1, "buggy file should exit 1");
        expect(r.out.find("SA004") != std::string::npos, "stdout should mention SA004");
        expect(r.out.find(file.string()) != std::string::npos, "stdout should mention the file path");
    }

    // select_filters_rules
    {
        std::filesystem::path dir = scratchDir();
        std::filesystem::path file = dir / "bad.c";
        writeFile(file, "int classify(int x) {\n    if (x > 0) {\n        return 1;\n    }\n}\n");

        RunResult r = run({"scan", file.string(), "--no-config", "--select", "SA001"});
        expect(r.exitCode == 0, "--select SA001 should filter out the SA004 finding, exiting 0");
        expect(r.out.empty(), "--select SA001 should produce no stdout for this file");
    }

    // show_source_adds_a_snippet_without_changing_default_output
    {
        std::filesystem::path dir = scratchDir();
        std::filesystem::path file = dir / "bad.c";
        writeFile(file, "int classify(int x) {\n    if (x > 0) {\n        return 1;\n    }\n}\n");

        RunResult withoutFlag = run({"scan", file.string(), "--no-config"});
        expect(withoutFlag.out.find("^") == std::string::npos, "default output should not contain a caret");

        RunResult withFlag = run({"scan", file.string(), "--no-config", "--show-source"});
        expect(withFlag.exitCode == withoutFlag.exitCode, "--show-source should not change the exit code");
        expect(withFlag.out.find("^") != std::string::npos, "--show-source output should contain a caret");
        expect(withFlag.out.find("SA004") != std::string::npos,
               "--show-source output should still contain rule codes");
    }

    // missing_path_exits_two
    {
        RunResult r = run({"scan", "/no/such/path.c", "--no-config"});
        expect(r.exitCode == 2, "a missing path should exit 2");
    }

    std::cout << "cli_test: all checks passed\n";
    return 0;
}
