#pragma once

#include "ast.h"
#include "token.h"

#include <string>
#include <vector>

namespace minic {

// Recursive-descent parser. Consumes a flat token stream (as produced by
// Lexer::tokenize()) and builds an AST. Throws std::runtime_error with a
// "file:line:col: error: ..." message on the first syntax error.
class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    ProgramNode parseProgram();

private:
    // Token stream helpers.
    const Token &peek(int offset = 0) const;
    const Token &advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    const Token &expect(TokenType type, const std::string &message);
    [[noreturn]] void error(const Token &token, const std::string &message) const;

    bool isTypeToken(TokenType type) const;
    // True for any token that can start a type: the builtin keywords, plus
    // struct/union/enum (which start a *reference* to a previously declared
    // tag, e.g. `struct Point p;` — not a new declaration).
    bool startsType(TokenType type) const;
    Type tokenToType(TokenType type) const;
    // Parses a type, including any struct/union tag reference and trailing
    // '*'s, but not an array-size suffix (see parseArraySuffix).
    Type parseType();
    // Parses an optional "[size]" suffix onto `base`, used by both variable
    // declarations and struct/union field declarations.
    Type parseArraySuffix(Type base);

    // Top-level.
    std::unique_ptr<FuncDefNode> parseFuncDef();
    ParamNode parseParam();
    std::unique_ptr<AggregateDeclNode> parseAggregateDecl(bool isUnion);
    FieldNode parseFieldDecl();
    std::unique_ptr<EnumDeclNode> parseEnumDecl();
    EnumeratorNode parseEnumerator(long long &nextValue);

    // Statements.
    std::unique_ptr<BlockStmtNode> parseBlock();
    StmtPtr parseStatement();
    std::unique_ptr<VarDeclStmtNode> parseVarDeclNoSemi();
    StmtPtr parseVarDecl();
    // Parses a single non-declaration "simple statement" without a trailing
    // semicolon: an assignment (plain or compound), or a bare expression
    // (e.g. `i++`) wrapped in ExprStmtNode. Shared by for-loop init/update
    // and (with its own semicolon handling) top-level statement parsing.
    StmtPtr parseSimpleStmtNoSemi();
    StmtPtr parseAssignOrExprStmt();
    StmtPtr parseIf();
    StmtPtr parseWhile();
    StmtPtr parseFor();
    StmtPtr parseForInit();
    StmtPtr parseForUpdate();
    StmtPtr parseReturn();
    StmtPtr parseBreak();
    StmtPtr parseContinue();
    StmtPtr parseDoWhile();
    StmtPtr parseSwitch();
    StmtPtr parseCaseLabel();
    StmtPtr parseDefaultLabel();
    StmtPtr parseGoto();
    StmtPtr parseLabel();

    // True for a binary operator's assignment form (+=, &=, etc.); maps it
    // to the BinaryOp it desugars through in a compound AssignStmtNode.
    bool isCompoundAssignToken(TokenType type) const;
    BinaryOp compoundAssignOp(TokenType type) const;

    // Expressions, ordered from lowest to highest precedence.
    ExprPtr parseExpression();
    ExprPtr parseTernary();
    ExprPtr parseLogicalOr();
    ExprPtr parseLogicalAnd();
    ExprPtr parseBitwiseOr();
    ExprPtr parseBitwiseXor();
    ExprPtr parseBitwiseAnd();
    ExprPtr parseEquality();
    ExprPtr parseComparison();
    ExprPtr parseShift();
    ExprPtr parseAdditive();
    ExprPtr parseMultiplicative();
    ExprPtr parseUnary();
    ExprPtr parsePostfix();
    ExprPtr parsePrimary();
    ExprPtr parseCallExpr(const Token &calleeTok);

    std::vector<Token> tokens_;
    std::size_t pos_ = 0;
};

} // namespace minic
