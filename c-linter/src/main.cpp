#include "diagnostic.h"
#include "linter.h"
#include "snippet.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "CLI11.hpp"

namespace {

struct Options {
    std::vector<std::string> inputPaths;
    int maxLineLength = 80;
    cl::BraceStyle braceStyle = cl::BraceStyle::KandR;
    bool showHelp = false;
    bool showSource = false;
};

const char *kUsage =
    "usage: c-lint <file> [<file>...] [--max-line-length=N] [--brace-style=kr|allman] "
    "[--show-source] [-h|--help]\n";

// Malformed flags (unknown option, bad --max-line-length/--brace-style value)
// throw here and are treated as exit code 1. A wholly missing input file
// list is NOT an exception -- it's reported by the caller as exit code 2,
// matching c-preprocess's/c-compiler's convention of distinguishing
// "used the tool wrong" from "the tool ran and found something".
Options parseArgs(int argc, char **argv) {
    CLI::App app{"c-lint"};
    // Positional-file collection and unknown-option detection are done by
    // hand below (via remaining()), and --max-line-length/--brace-style
    // are validated by hand too, so the existing error text ("unknown
    // option: ...", "invalid --max-line-length value: ...", "invalid
    // --brace-style value: ...") stays exactly what cli_test.cpp asserts
    // on -- CLI11 only drives the actual tokenizing/flag-matching here.
    app.allow_extras(true);
    app.set_help_flag(); // disable CLI11's own auto --help; -h/--help
                          // below just sets a bool instead of throwing.

    Options opts;
    app.add_flag("-h,--help", opts.showHelp);
    app.add_flag("--show-source", opts.showSource);

    std::string maxLineLengthValue;
    std::string braceStyleValue;
    app.add_option("--max-line-length", maxLineLengthValue);
    app.add_option("--brace-style", braceStyleValue);

    std::vector<std::string> args(argv + 1, argv + argc);
    std::reverse(args.begin(), args.end()); // CLI11's vector overload consumes from the back
    try {
        app.parse(args);
    } catch (const CLI::ParseError &e) {
        throw std::runtime_error(e.what());
    }

    if (opts.showHelp) return opts;

    if (!maxLineLengthValue.empty()) {
        int parsed = 0;
        bool ok = true;
        try {
            std::size_t consumed = 0;
            parsed = std::stoi(maxLineLengthValue, &consumed);
            if (consumed != maxLineLengthValue.size()) ok = false;
        } catch (const std::exception &) {
            ok = false;
        }
        if (!ok || parsed <= 0) {
            throw std::runtime_error("invalid --max-line-length value: " + maxLineLengthValue);
        }
        opts.maxLineLength = parsed;
    }

    if (!braceStyleValue.empty()) {
        if (braceStyleValue == "kr") {
            opts.braceStyle = cl::BraceStyle::KandR;
        } else if (braceStyleValue == "allman") {
            opts.braceStyle = cl::BraceStyle::Allman;
        } else {
            throw std::runtime_error("invalid --brace-style value: " + braceStyleValue);
        }
    }

    for (const std::string &arg : app.remaining()) {
        if (arg.size() > 1 && arg[0] == '-') {
            throw std::runtime_error("unknown option: " + arg);
        }
        opts.inputPaths.push_back(arg);
    }

    return opts;
}

bool readFile(const std::string &path, std::string &outContents) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    outContents = ss.str();
    return true;
}

// Splits on '\n' without keeping the newline; a trailing partial line (no
// final '\n') is still included, matching how diagnostics reference it.
std::vector<std::string> splitLines(const std::string &contents) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start <= contents.size()) {
        std::size_t end = contents.find('\n', start);
        if (end == std::string::npos) {
            lines.push_back(contents.substr(start));
            break;
        }
        lines.push_back(contents.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

} // namespace

int main(int argc, char **argv) {
    Options opts;
    try {
        opts = parseArgs(argc, argv);
    } catch (const std::exception &ex) {
        std::cerr << "error: " << ex.what() << "\n" << kUsage;
        return 1;
    }

    if (opts.showHelp) {
        std::cout << kUsage;
        return 0;
    }
    if (opts.inputPaths.empty()) {
        std::cerr << kUsage;
        return 2;
    }

    cl::LinterOptions linterOptions;
    linterOptions.maxLineLength = opts.maxLineLength;
    linterOptions.braceStyle = opts.braceStyle;
    cl::Linter linter(linterOptions);

    bool hadFindings = false;
    for (const std::string &path : opts.inputPaths) {
        std::string contents;
        if (!readFile(path, contents)) {
            std::cerr << "error: cannot open \"" << path << "\"\n";
            hadFindings = true;
            continue;
        }

        std::vector<cl::Diagnostic> diagnostics = linter.lintSource(contents, path);
        std::vector<std::string> lines = opts.showSource ? splitLines(contents) : std::vector<std::string>{};
        for (const cl::Diagnostic &diagnostic : diagnostics) {
            if (opts.showSource && diagnostic.line >= 1 &&
                static_cast<std::size_t>(diagnostic.line) <= lines.size()) {
                std::cout << cl::formatWithSource(diagnostic, lines[diagnostic.line - 1]) << "\n";
            } else {
                std::cout << cl::format(diagnostic) << "\n";
            }
        }
        if (!diagnostics.empty()) hadFindings = true;
    }

    return hadFindings ? 1 : 0;
}
