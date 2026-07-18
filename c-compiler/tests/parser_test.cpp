#include "ast.h"
#include "lexer.h"
#include "parser.h"

#include <cassert>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

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

minic::ProgramNode parseSource(const std::string &source, const std::string &filename = "<input>") {
    minic::Lexer lexer(source, filename);
    minic::Parser parser(lexer.tokenize());
    return parser.parseProgram();
}

minic::ProgramNode parseFile(const std::string &path) {
    return parseSource(readFile(path), path);
}

std::set<std::string> functionNames(const minic::ProgramNode &program) {
    std::set<std::string> names;
    for (const auto &func : program.functions) {
        names.insert(func->name);
    }
    return names;
}

} // namespace

int main() {
    const std::string examplesDir = MINIC_EXAMPLES_DIR;

    // Each example program should parse and print without error.
    {
        auto program = parseFile(examplesDir + "/fibonacci.mc");
        const auto names = functionNames(program);
        assert(names.count("fibonacci") == 1);
        assert(names.count("main") == 1);

        std::ostringstream out;
        program.print(out);
        const std::string ast = out.str();
        assert(ast.find("FuncDef fibonacci -> int") != std::string::npos);
        assert(ast.find("Call fibonacci") != std::string::npos);
        assert(ast.find("BinOp <=") != std::string::npos);
    }

    {
        auto program = parseFile(examplesDir + "/gcd.mc");
        const auto names = functionNames(program);
        assert(names.count("gcd") == 1);
        assert(names.count("main") == 1);
    }

    {
        auto program = parseFile(examplesDir + "/fizzbuzz.mc");
        assert(functionNames(program).count("main") == 1);
    }

    {
        auto program = parseFile(examplesDir + "/sum_of_squares.mc");
        const auto names = functionNames(program);
        assert(names.count("sum_of_squares") == 1);
        assert(names.count("main") == 1);
    }

    // A program that exercises every statement and expression kind.
    {
        const std::string source =
            "int max(int a, int b) {\n"
            "    if (a > b) {\n"
            "        return a;\n"
            "    } else {\n"
            "        return b;\n"
            "    }\n"
            "}\n"
            "\n"
            "int main() {\n"
            "    int total = 0;\n"
            "    for (int i = 0; i < 10; i = i + 1) {\n"
            "        if (i == 5) {\n"
            "            continue;\n"
            "        }\n"
            "        if (i == 8) {\n"
            "            break;\n"
            "        }\n"
            "        total = total + max(i, 1) * 2 - 1;\n"
            "    }\n"
            "    printf(\"%d\\n\", total);\n"
            "    return 0;\n"
            "}\n";

        auto program = parseSource(source);
        assert(program.functions.size() == 2);

        std::ostringstream out;
        program.print(out);
        const std::string ast = out.str();
        assert(ast.find("FuncDef max -> int") != std::string::npos);
        assert(ast.find("Param int a") != std::string::npos);
        assert(ast.find("For") != std::string::npos);
        assert(ast.find("Break") != std::string::npos);
        assert(ast.find("Continue") != std::string::npos);
        assert(ast.find("BinOp ==") != std::string::npos);
        assert(ast.find("BinOp *") != std::string::npos);
        assert(ast.find("Call printf") != std::string::npos);
        assert(ast.find("StringLit") != std::string::npos);
    }

    // Operator precedence: "*" binds tighter than "+", "&&" binds tighter than "||".
    {
        auto program = parseSource(
            "int main() {\n"
            "    int x = 1 + 2 * 3;\n"
            "    int y = 1 || 2 && 3;\n"
            "    return x;\n"
            "}\n");

        const auto &body = program.functions[0]->body->statements;
        const auto *xDecl = dynamic_cast<minic::VarDeclStmtNode *>(body[0].get());
        assert(xDecl != nullptr);
        const auto *xAdd = dynamic_cast<minic::BinOpExprNode *>(xDecl->init.get());
        assert(xAdd != nullptr && xAdd->op == minic::BinaryOp::Add);
        assert(dynamic_cast<minic::BinOpExprNode *>(xAdd->rhs.get())->op == minic::BinaryOp::Mul);

        const auto *yDecl = dynamic_cast<minic::VarDeclStmtNode *>(body[1].get());
        assert(yDecl != nullptr);
        const auto *yOr = dynamic_cast<minic::BinOpExprNode *>(yDecl->init.get());
        assert(yOr != nullptr && yOr->op == minic::BinaryOp::Or);
        assert(dynamic_cast<minic::BinOpExprNode *>(yOr->rhs.get())->op == minic::BinaryOp::And);
    }

    // Unary operators and parenthesized expressions.
    {
        auto program = parseSource(
            "int main() {\n"
            "    int x = -(1 + 2);\n"
            "    int y = !x;\n"
            "    return x;\n"
            "}\n");

        const auto &body = program.functions[0]->body->statements;
        const auto *xDecl = dynamic_cast<minic::VarDeclStmtNode *>(body[0].get());
        const auto *neg = dynamic_cast<minic::UnaryOpExprNode *>(xDecl->init.get());
        assert(neg != nullptr && neg->op == minic::UnaryOp::Negate);
        assert(dynamic_cast<minic::BinOpExprNode *>(neg->operand.get()) != nullptr);

        const auto *yDecl = dynamic_cast<minic::VarDeclStmtNode *>(body[1].get());
        const auto *notExpr = dynamic_cast<minic::UnaryOpExprNode *>(yDecl->init.get());
        assert(notExpr != nullptr && notExpr->op == minic::UnaryOp::Not);
    }

    // Pointer declarations, address-of, dereference, and assignment through
    // a dereferenced pointer.
    {
        auto program = parseSource(
            "void swap(int *a, int *b) {\n"
            "    int temp = *a;\n"
            "    *a = *b;\n"
            "    *b = temp;\n"
            "}\n"
            "\n"
            "int main() {\n"
            "    int x = 1;\n"
            "    int *p = &x;\n"
            "    swap(&x, p);\n"
            "    return *p;\n"
            "}\n");
        assert(program.functions.size() == 2);

        const auto &swapParams = program.functions[0]->params;
        assert(swapParams.size() == 2);
        assert(swapParams[0].type.isPointer());
        assert(swapParams[0].type.pointerDepth() == 1);

        const auto &mainBody = program.functions[1]->body->statements;
        const auto *pDecl = dynamic_cast<minic::VarDeclStmtNode *>(mainBody[1].get());
        assert(pDecl != nullptr && pDecl->type.isPointer());
        const auto *addressOf = dynamic_cast<minic::UnaryOpExprNode *>(pDecl->init.get());
        assert(addressOf != nullptr && addressOf->op == minic::UnaryOp::AddressOf);

        std::ostringstream out;
        program.print(out);
        const std::string ast = out.str();
        assert(ast.find("Param int* a") != std::string::npos);
        assert(ast.find("UnaryOp &") != std::string::npos);
        assert(ast.find("UnaryOp *") != std::string::npos);

        const auto &swapBody = program.functions[0]->body->statements;
        const auto *derefAssign = dynamic_cast<minic::AssignStmtNode *>(swapBody[1].get());
        assert(derefAssign != nullptr);
        const auto *derefTarget = dynamic_cast<minic::UnaryOpExprNode *>(derefAssign->target.get());
        assert(derefTarget != nullptr && derefTarget->op == minic::UnaryOp::Deref);
    }

    // Array declarations and indexing (read, write, address-of an element).
    {
        auto program = parseSource(
            "int main() {\n"
            "    int arr[5];\n"
            "    arr[0] = 1;\n"
            "    int x = arr[0];\n"
            "    int *p = &arr[1];\n"
            "    return x;\n"
            "}\n");
        assert(program.functions.size() == 1);

        const auto &body = program.functions[0]->body->statements;
        const auto *arrDecl = dynamic_cast<minic::VarDeclStmtNode *>(body[0].get());
        assert(arrDecl != nullptr);
        assert(arrDecl->type.isArray() && arrDecl->type.arrayLength() == 5);

        const auto *indexAssign = dynamic_cast<minic::AssignStmtNode *>(body[1].get());
        assert(indexAssign != nullptr);
        const auto *indexTarget = dynamic_cast<minic::IndexExprNode *>(indexAssign->target.get());
        assert(indexTarget != nullptr);
        assert(dynamic_cast<minic::IdentExprNode *>(indexTarget->base.get()) != nullptr);

        const auto *xDecl = dynamic_cast<minic::VarDeclStmtNode *>(body[2].get());
        assert(xDecl != nullptr);
        assert(dynamic_cast<minic::IndexExprNode *>(xDecl->init.get()) != nullptr);

        std::ostringstream out;
        program.print(out);
        const std::string ast = out.str();
        assert(ast.find("VarDecl int[5] arr") != std::string::npos);
        assert(ast.find("Index") != std::string::npos);
    }

    // Struct/union/enum declarations, member access (. and ->), and the
    // function-return-type-vs-definition disambiguation at top level.
    {
        auto program = parseSource(
            "struct Point {\n"
            "    int x;\n"
            "    int y;\n"
            "};\n"
            "\n"
            "enum Color {\n"
            "    RED,\n"
            "    GREEN,\n"
            "    BLUE = 10\n"
            "};\n"
            "\n"
            "union Number {\n"
            "    int i;\n"
            "    float f;\n"
            "};\n"
            "\n"
            "struct Point makeOrigin() {\n"
            "    struct Point p;\n"
            "    p.x = 0;\n"
            "    return p;\n"
            "}\n"
            "\n"
            "int main() {\n"
            "    struct Point p = makeOrigin();\n"
            "    struct Point *pp = &p;\n"
            "    int x = pp->x;\n"
            "    int c = GREEN;\n"
            "    return x + c;\n"
            "}\n");

        assert(program.aggregates.size() == 2);
        assert(program.enums.size() == 1);
        assert(program.functions.size() == 2);

        const auto *pointDecl = program.aggregates[0].get();
        assert(pointDecl->name == "Point" && !pointDecl->isUnion && pointDecl->fields.size() == 2);
        const auto *numberDecl = program.aggregates[1].get();
        assert(numberDecl->name == "Number" && numberDecl->isUnion);

        assert(program.enums[0]->enumerators.size() == 3);
        assert(program.enums[0]->enumerators[2].name == "BLUE" && program.enums[0]->enumerators[2].value == 10);

        // makeOrigin's return type is a struct reference, not a redefinition.
        assert(program.functions[0]->name == "makeOrigin");
        assert(program.functions[0]->returnType.isStruct());

        const auto &mainBody = program.functions[1]->body->statements;
        const auto *ppDecl = dynamic_cast<minic::VarDeclStmtNode *>(mainBody[1].get());
        assert(ppDecl != nullptr && ppDecl->type.isPointer());

        const auto *xDecl = dynamic_cast<minic::VarDeclStmtNode *>(mainBody[2].get());
        const auto *member = dynamic_cast<minic::MemberExprNode *>(xDecl->init.get());
        assert(member != nullptr && member->field == "x");
        // `pp->x` desugars to `(*pp).x`.
        const auto *deref = dynamic_cast<minic::UnaryOpExprNode *>(member->base.get());
        assert(deref != nullptr && deref->op == minic::UnaryOp::Deref);

        std::ostringstream out;
        program.print(out);
        const std::string ast = out.str();
        assert(ast.find("Struct Point") != std::string::npos);
        assert(ast.find("Union Number") != std::string::npos);
        assert(ast.find("Enum Color") != std::string::npos);
        assert(ast.find("Member x") != std::string::npos);
    }

    // Bitwise operator precedence: "&" binds tighter than "^", which binds
    // tighter than "|"; "<<"/">>" sit between comparison and additive.
    {
        auto program = parseSource(
            "int main() {\n"
            "    int x = 1 | 2 ^ 3 & 4;\n"
            "    int y = 1 + 2 << 3;\n"
            "    return x;\n"
            "}\n");

        const auto &body = program.functions[0]->body->statements;
        const auto *xDecl = dynamic_cast<minic::VarDeclStmtNode *>(body[0].get());
        const auto *xOr = dynamic_cast<minic::BinOpExprNode *>(xDecl->init.get());
        assert(xOr != nullptr && xOr->op == minic::BinaryOp::BitOr);
        const auto *xXor = dynamic_cast<minic::BinOpExprNode *>(xOr->rhs.get());
        assert(xXor != nullptr && xXor->op == minic::BinaryOp::BitXor);
        assert(dynamic_cast<minic::BinOpExprNode *>(xXor->rhs.get())->op == minic::BinaryOp::BitAnd);

        const auto *yDecl = dynamic_cast<minic::VarDeclStmtNode *>(body[1].get());
        const auto *yShl = dynamic_cast<minic::BinOpExprNode *>(yDecl->init.get());
        assert(yShl != nullptr && yShl->op == minic::BinaryOp::Shl);
        assert(dynamic_cast<minic::BinOpExprNode *>(yShl->lhs.get())->op == minic::BinaryOp::Add);
    }

    // Ternary, prefix/postfix increment, compound assignment, and the
    // comma operator (only valid inside explicit parens).
    {
        auto program = parseSource(
            "int main() {\n"
            "    int a = 1;\n"
            "    int b = a > 0 ? a : -a;\n"
            "    a++;\n"
            "    ++a;\n"
            "    a += 2;\n"
            "    int c = (a++, b++, a + b);\n"
            "    return b;\n"
            "}\n");

        const auto &body = program.functions[0]->body->statements;
        const auto *bDecl = dynamic_cast<minic::VarDeclStmtNode *>(body[1].get());
        assert(dynamic_cast<minic::TernaryExprNode *>(bDecl->init.get()) != nullptr);

        const auto *postStmt = dynamic_cast<minic::ExprStmtNode *>(body[2].get());
        const auto *postInc = dynamic_cast<minic::IncDecExprNode *>(postStmt->expr.get());
        assert(postInc != nullptr && postInc->isIncrement && !postInc->isPrefix);

        const auto *preStmt = dynamic_cast<minic::ExprStmtNode *>(body[3].get());
        const auto *preInc = dynamic_cast<minic::IncDecExprNode *>(preStmt->expr.get());
        assert(preInc != nullptr && preInc->isIncrement && preInc->isPrefix);

        const auto *compound = dynamic_cast<minic::AssignStmtNode *>(body[4].get());
        assert(compound != nullptr && compound->compoundOp.has_value() &&
               *compound->compoundOp == minic::BinaryOp::Add);

        const auto *cDecl = dynamic_cast<minic::VarDeclStmtNode *>(body[5].get());
        const auto *outerComma = dynamic_cast<minic::BinOpExprNode *>(cDecl->init.get());
        assert(outerComma != nullptr && outerComma->op == minic::BinaryOp::Comma);
    }

    // do-while, switch/case/default (with fallthrough), and goto/labels.
    {
        auto program = parseSource(
            "int main() {\n"
            "    int i = 0;\n"
            "    do {\n"
            "        i++;\n"
            "    } while (i < 3);\n"
            "\n"
            "    switch (i) {\n"
            "    case 0:\n"
            "    case 1:\n"
            "        i = 1;\n"
            "        break;\n"
            "    default:\n"
            "        i = 2;\n"
            "    }\n"
            "\n"
            "    int n = 0;\n"
            "top:\n"
            "    n++;\n"
            "    if (n < 3) {\n"
            "        goto top;\n"
            "    }\n"
            "    return n;\n"
            "}\n");

        const auto &body = program.functions[0]->body->statements;
        const auto *doWhile = dynamic_cast<minic::DoWhileStmtNode *>(body[1].get());
        assert(doWhile != nullptr);
        assert(dynamic_cast<minic::BinOpExprNode *>(doWhile->condition.get())->op == minic::BinaryOp::Lt);

        const auto *switchStmt = dynamic_cast<minic::SwitchStmtNode *>(body[2].get());
        assert(switchStmt != nullptr);
        const auto &switchBody = switchStmt->body->statements;
        assert(dynamic_cast<minic::CaseLabelStmtNode *>(switchBody[0].get())->value == 0);
        assert(dynamic_cast<minic::CaseLabelStmtNode *>(switchBody[1].get())->value == 1);
        assert(dynamic_cast<minic::DefaultLabelStmtNode *>(switchBody[4].get()) != nullptr);

        const auto *label = dynamic_cast<minic::LabelStmtNode *>(body[4].get());
        assert(label != nullptr && label->name == "top");

        std::ostringstream out;
        program.print(out);
        const std::string ast = out.str();
        assert(ast.find("DoWhile") != std::string::npos);
        assert(ast.find("Switch") != std::string::npos);
        assert(ast.find("Case 0") != std::string::npos);
        assert(ast.find("Default") != std::string::npos);
        assert(ast.find("Label top") != std::string::npos);
        assert(ast.find("Goto top") != std::string::npos);
    }

    // A negative case label.
    {
        auto program = parseSource("int main() {\n"
                                    "    switch (1) {\n"
                                    "    case -1:\n"
                                    "        return 0;\n"
                                    "    }\n"
                                    "    return 1;\n"
                                    "}\n");
        const auto *switchStmt = dynamic_cast<minic::SwitchStmtNode *>(program.functions[0]->body->statements[0].get());
        assert(dynamic_cast<minic::CaseLabelStmtNode *>(switchStmt->body->statements[0].get())->value == -1);
    }

    // Syntax errors are reported with file:line:column.
    {
        bool threw = false;
        try {
            parseSource("int main() { return 0 }", "bad.mc");
        } catch (const std::exception &ex) {
            threw = true;
            const std::string message = ex.what();
            assert(message.find("bad.mc:1:") != std::string::npos);
        }
        assert(threw);
    }

    // `volatile` is an explicit non-goal (concurrency-adjacent scope creep)
    // and gets a targeted diagnostic rather than a generic parse error.
    {
        bool threw = false;
        try {
            parseSource("int main() { volatile int x; return 0; }");
        } catch (const std::exception &ex) {
            threw = true;
            const std::string message = ex.what();
            assert(message.find("not supported") != std::string::npos);
        }
        assert(threw);
    }

    // `(type)expr` casts parse as a CastExprNode, right-associatively
    // chaining when repeated, and don't disturb plain parenthesized
    // expressions (no type keyword right after '(').
    {
        auto program = parseSource("int main() {\n"
                                    "    float f = 1.5;\n"
                                    "    int x = (int)f;\n"
                                    "    int y = (int)(float)x;\n"
                                    "    int z = (x + 1);\n"
                                    "    return 0;\n"
                                    "}\n");
        std::ostringstream out;
        program.print(out);
        const std::string ast = out.str();
        assert(ast.find("Cast <int>") != std::string::npos);
        // Two separate `Cast <int>` sites: `(int)f` and the outer cast of
        // `(int)(float)x`; a plain parenthesized `(x + 1)` should not
        // itself print as any Cast node.
        std::size_t firstCast = ast.find("Cast <int>");
        std::size_t secondCast = ast.find("Cast <int>", firstCast + 1);
        assert(secondCast != std::string::npos);
        assert(ast.find("Cast <float>") != std::string::npos);
    }

    return 0;
}
