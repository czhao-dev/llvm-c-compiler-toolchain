#include "codegen.h"
#include "lexer.h"
#include "parser.h"
#include "sema.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>

#ifndef MINIC_EXAMPLES_DIR
#define MINIC_EXAMPLES_DIR "examples"
#endif

namespace {

std::string readFile(const std::string &path) {
    std::ifstream file(path);
    assert(file && "could not open example file");
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// Compile sourcePath through the full MiniC pipeline, run the resulting
// binary, and return its stdout. The binary is removed after execution.
std::string compileAndRun(const std::string &sourcePath) {
    const std::string source = readFile(sourcePath);
    minic::Lexer lexer(source, sourcePath);
    minic::Parser parser(lexer.tokenize());
    const minic::ProgramNode program = parser.parseProgram();

    minic::SemanticAnalyzer sema;
    for (const auto &diag : sema.analyze(program)) {
        if (diag.severity == minic::DiagnosticSeverity::Error) {
            throw std::runtime_error(diag.toString());
        }
    }

    // Get a unique path without leaving a non-executable placeholder file.
    const std::string tempTemplate =
        (std::filesystem::temp_directory_path() / "minic_codegen_test_XXXXXX").string();
    std::vector<char> buf(tempTemplate.begin(), tempTemplate.end());
    buf.push_back('\0');
    const int fd = mkstemp(buf.data());
    assert(fd != -1);
    close(fd);
    std::filesystem::remove(buf.data());
    const std::string outPath(buf.data());

    minic::compileToNative(program, outPath, sourcePath);

    FILE *pipe = popen(outPath.c_str(), "r");
    assert(pipe && "failed to run compiled binary");
    std::string output;
    char line[256];
    while (fgets(line, sizeof(line), pipe)) {
        output += line;
    }
    pclose(pipe);
    std::filesystem::remove(outPath);
    return output;
}

} // namespace

int main() {
#if !defined(MINIC_HAS_LLVM)
    return 0;
#else
    const std::string dir = MINIC_EXAMPLES_DIR;

    assert(compileAndRun(dir + "/fibonacci.mc") == "0\n1\n1\n2\n3\n5\n8\n13\n21\n34\n");
    assert(compileAndRun(dir + "/gcd.mc") == "21\n");
    assert(compileAndRun(dir + "/sum_of_squares.mc") == "338350.000000\n");
    assert(compileAndRun(dir + "/pointer_swap.mc") ==
           "before: x=3 y=7\nafter: x=7 y=3\nincremented: 8\np is non-null\n");
    assert(compileAndRun(dir + "/array_sum.mc") == "0\n1\n4\n9\n16\nsum=30\n");
    assert(compileAndRun(dir + "/struct_point.mc") == "(5, 3)\n");
    assert(compileAndRun(dir + "/bit_ops.mc") ==
           "0 has 0 bits set\n7 has 3 bits set\n14 has 3 bits set\n21 has 3 bits set\n28 has 3 bits set\n"
           "total=12\nclamped=12\n");
    assert(compileAndRun(dir + "/control_flow.mc") ==
           "classify(0)=0\nclassify(1)=1\nclassify(2)=1\nclassify(3)=-1\nn=3\ncount=9\n");

    const std::string fizz = compileAndRun(dir + "/fizzbuzz.mc");
    assert(fizz.substr(0, 2) == "1\n");
    assert(fizz.find("FizzBuzz\n") != std::string::npos);
    assert(!fizz.empty() && fizz.back() == '\n');

    return 0;
#endif
}
