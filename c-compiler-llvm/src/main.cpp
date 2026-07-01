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
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(std::cout);
            std::exit(0);
        }
        if (arg == "--version") {
            options.showVersion = true;
            continue;
        }
        if (arg == "--emit-tokens") {
            options.emitTokens = true;
            continue;
        }
        if (arg == "--emit-ast") {
            options.emitAst = true;
            continue;
        }
        if (arg == "--emit-ir") {
            options.emitIr = true;
            continue;
        }
        if (arg == "-O0" || arg == "-O1" || arg == "-O2" || arg == "-O3") {
            options.optLevel = arg[2] - '0';
            continue;
        }
        if (arg == "-o") {
            if (i + 1 >= argc) {
                throw std::runtime_error("-o requires an output path");
            }
            options.outputPath = argv[++i];
            continue;
        }
        if (!arg.empty() && arg[0] == '-') {
            throw std::runtime_error("unknown option: " + arg);
        }
        if (!options.inputPath.empty()) {
            throw std::runtime_error("multiple input files were provided");
        }
        options.inputPath = arg;
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
