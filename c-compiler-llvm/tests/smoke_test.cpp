#include "codegen.h"
#include "lexer.h"
#include "parser.h"
#include "sema.h"

#include <cassert>
#include <string>

int main() {
    minic::Lexer lexer("int main() { return 0; }");
    const auto tokens = lexer.tokenize();
    assert(!tokens.empty());
    assert(tokens.back().type == minic::TokenType::EndOfFile);

    minic::Parser parser(tokens);
    const auto program = parser.parseProgram();
    assert(program.functions.size() == 1);
    assert(program.functions[0]->name == "main");

    assert(minic::semanticAnalyzerStatus().find("implemented") != std::string::npos);
    assert(!minic::codegenStatus().empty());
    return 0;
}
