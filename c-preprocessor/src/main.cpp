#include "diagnostics.h"
#include "preprocessor.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "CLI11.hpp"

namespace {

struct Options {
    std::string inputPath; // empty ⇒ no input file was given
    std::string outputPath; // empty ⇒ write to stdout
    std::vector<std::string> includeDirs;
    bool showHelp = false;
};

const char *kUsage = "usage: c-preprocess <input-file> [-I <dir>]... [-o <output-file>] [-h|--help]\n";

// Malformed flags (unknown option, -I/-o missing its argument, more than
// one positional argument) throw here and are treated as exit code 1.
// A wholly missing input file is NOT an exception — it's reported by the
// caller as exit code 2, matching c-compiler's CLI convention of
// distinguishing "used the tool wrong" from "the tool ran and failed".
Options parseArgs(int argc, char **argv) {
    CLI::App app{"c-preprocess"};
    // Positional-file handling and unknown-option detection are done by
    // hand below (via remaining()) rather than through CLI11's own
    // positional/extras machinery, to keep this tool's existing error text
    // ("unknown option: ...", "multiple input files were provided")
    // exactly as cli_test.cpp already asserts on.
    app.allow_extras(true);
    app.set_help_flag(); // disable CLI11's own auto --help so -h/--help
                          // just sets a bool below instead of throwing.

    Options opts;
    app.add_flag("-h,--help", opts.showHelp);
    app.add_option("-I", opts.includeDirs);
    app.add_option("-o", opts.outputPath);

    std::vector<std::string> args(argv + 1, argv + argc);
    std::reverse(args.begin(), args.end()); // CLI11's vector overload consumes from the back
    try {
        app.parse(args);
    } catch (const CLI::ParseError &e) {
        throw std::runtime_error(e.what());
    }

    if (opts.showHelp) return opts;

    std::vector<std::string> positionals = app.remaining();
    for (const std::string &arg : positionals) {
        if (arg.size() > 1 && arg[0] == '-') {
            throw std::runtime_error("unknown option: " + arg);
        }
    }
    if (positionals.size() > 1) {
        throw std::runtime_error("multiple input files were provided");
    }
    if (!positionals.empty()) {
        opts.inputPath = positionals.front();
    }
    return opts;
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
    if (opts.inputPath.empty()) {
        std::cerr << kUsage;
        return 2;
    }

    try {
        pp::PreprocessorOptions ppOptions;
        ppOptions.includeSearchPaths = opts.includeDirs;
        pp::Preprocessor preprocessor(ppOptions);
        std::string result = preprocessor.preprocessFile(opts.inputPath);

        if (opts.outputPath.empty()) {
            std::cout << result;
        } else {
            std::ofstream out(opts.outputPath, std::ios::binary);
            if (!out) {
                std::cerr << "error: cannot open output file \"" << opts.outputPath << "\"\n";
                return 1;
            }
            out << result;
        }
        return 0;
    } catch (const pp::PreprocessorError &ex) {
        std::cerr << ex.what() << "\n";
        return 1;
    } catch (const std::exception &ex) {
        std::cerr << "error: " << ex.what() << "\n";
        return 1;
    }
}
