#include "codegen.h"
#include "lexer.h"
#include "parser.h"
#include "sema.h"

#include <fstream>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "CLI11.hpp"

namespace {

struct Options {
    std::string inputPath;
    std::string outputPath = "a.out";
    bool emitTokens = false;
    bool emitAst = false;
    bool emitIr = false;
    bool showVersion = false;
    int optLevel = 0;
};

std::string readFile(const std::string &path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("could not open input file: " + path);
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void printUsage(std::ostream &out) {
    out << "usage: minic <source.mc> [--emit-tokens] [--emit-ast] [--emit-ir]\n"
        << "                         [-O0|-O1|-O2|-O3] [-o output]\n";
}

// Runs the semantic analyzer and prints its diagnostics to stderr. Returns 1
// if any diagnostic is an error, 0 otherwise.
int runSemanticAnalysis(const minic::ProgramNode &program) {
    minic::SemanticAnalyzer analyzer;
    const auto diagnostics = analyzer.analyze(program);

    bool hasErrors = false;
    for (const auto &diag : diagnostics) {
        std::cerr << diag.toString() << '\n';
        hasErrors = hasErrors || diag.severity == minic::DiagnosticSeverity::Error;
    }
    return hasErrors ? 1 : 0;
}

Options parseArgs(int argc, char **argv) {
    CLI::App app{"minic"};
    // Positional-input, unknown-option, and -O0..-O3 handling all stay
    // hand-written on top of remaining(). -O0..-O3 specifically can't be
    // registered as CLI11 flags: CLI11 requires single-dash names to be
    // exactly one character (multi-character names need a double dash),
    // so "-O0" is rejected at registration time -- and the differential-
    // testing/benchmark harnesses invoke this CLI with that exact literal
    // token, so the syntax can't change.
    app.allow_extras(true);
    app.set_help_flag(); // disable CLI11's own auto --help; -h/--help
                          // below prints this tool's own usage text.

    bool help = false;
    app.add_flag("-h,--help", help);

    Options options;
    app.add_flag("--version", options.showVersion);
    app.add_flag("--emit-tokens", options.emitTokens);
    app.add_flag("--emit-ast", options.emitAst);
    app.add_flag("--emit-ir", options.emitIr);
    app.add_option("-o", options.outputPath);

    std::vector<std::string> args(argv + 1, argv + argc);
    std::vector<std::string> reversed(args.rbegin(), args.rend()); // CLI11's vector overload consumes from the back
    try {
        app.parse(reversed);
    } catch (const CLI::ParseError &e) {
        throw std::runtime_error(e.what());
    }

    if (help) {
        printUsage(std::cout);
        std::exit(0);
    }

    for (const std::string &arg : app.remaining()) {
        if (arg == "-O0" || arg == "-O1" || arg == "-O2" || arg == "-O3") {
            options.optLevel = arg[2] - '0';
        } else if (!arg.empty() && arg[0] == '-') {
            throw std::runtime_error("unknown option: " + arg);
        } else if (!options.inputPath.empty()) {
            throw std::runtime_error("multiple input files were provided");
        } else {
            options.inputPath = arg;
        }
    }
    return options;
}

} // namespace

int main(int argc, char **argv) {
    try {
        const Options options = parseArgs(argc, argv);
        if (options.showVersion) {
            std::cout << "MiniC compiler starter\n";
            std::cout << minic::codegenStatus() << '\n';
            return 0;
        }

        if (options.inputPath.empty()) {
            printUsage(std::cerr);
            return 2;
        }

        const std::string source = readFile(options.inputPath);

        if (options.emitTokens) {
            minic::Lexer lexer(source, options.inputPath);
            for (const auto &token : lexer.tokenize()) {
                std::cout << token << '\n';
            }
            return 0;
        }

        if (options.emitAst) {
            minic::Lexer lexer(source, options.inputPath);
            minic::Parser parser(lexer.tokenize());
            const minic::ProgramNode program = parser.parseProgram();
            const int semaStatus = runSemanticAnalysis(program);
            program.print(std::cout);
            return semaStatus;
        }

        if (options.emitIr) {
            minic::Lexer lexer(source, options.inputPath);
            minic::Parser parser(lexer.tokenize());
            const minic::ProgramNode program = parser.parseProgram();
            const int semaStatus = runSemanticAnalysis(program);
            if (semaStatus != 0) {
                return semaStatus;
            }
            std::cout << minic::emitLLVMIR(program, options.inputPath, options.optLevel);
            return 0;
        }

        {
            minic::Lexer lexer(source, options.inputPath);
            minic::Parser parser(lexer.tokenize());
            const minic::ProgramNode program = parser.parseProgram();
            const int semaStatus = runSemanticAnalysis(program);

            if (semaStatus == 0) {
                minic::compileToNative(program, options.outputPath, options.inputPath, options.optLevel);
                std::cout << "wrote " << options.outputPath << '\n';
            }
            return semaStatus;
        }
    } catch (const std::exception &ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
