#include "parser.h"

#include <sstream>
#include <stdexcept>
#include <utility>

namespace minic {
namespace {

char decodeCharLiteral(const std::string &lexeme) {
    if (lexeme.size() == 1) {
        return lexeme[0];
    }
    // Two-character escape sequence: lexeme[0] == '\\'.
    switch (lexeme[1]) {
    case 'n': return '\n';
    case 't': return '\t';
    case 'r': return '\r';
    case '0': return '\0';
    case '\\': return '\\';
    case '\'': return '\'';
    case '"': return '"';
    default: return lexeme[1];
    }
}

std::string decodeStringLiteral(const std::string &lexeme) {
    std::string decoded;
    for (std::size_t i = 0; i < lexeme.size(); ++i) {
        if (lexeme[i] != '\\' || i + 1 >= lexeme.size()) {
            decoded.push_back(lexeme[i]);
            continue;
        }

        const char escape = lexeme[++i];
        switch (escape) {
        case 'n': decoded.push_back('\n'); break;
        case 't': decoded.push_back('\t'); break;
        case 'r': decoded.push_back('\r'); break;
        case '0': decoded.push_back('\0'); break;
        case '\\': decoded.push_back('\\'); break;
        case '\'': decoded.push_back('\''); break;
        case '"': decoded.push_back('"'); break;
        default: decoded.push_back(escape); break;
        }
    }
    return decoded;
}

} // namespace

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {
    if (tokens_.empty()) {
        tokens_.push_back(Token{TokenType::EndOfFile, "", SourceLocation{}});
    }
}

// ---------------------------------------------------------------------------
// Token stream helpers
// ---------------------------------------------------------------------------

const Token &Parser::peek(int offset) const {
    std::size_t index = pos_ + static_cast<std::size_t>(offset);
    if (index >= tokens_.size()) {
        index = tokens_.size() - 1;
    }
    return tokens_[index];
}

const Token &Parser::advance() {
    const Token &tok = tokens_[pos_];
    if (pos_ + 1 < tokens_.size()) {
        ++pos_;
    }
    return tok;
}

bool Parser::check(TokenType type) const {
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

const Token &Parser::expect(TokenType type, const std::string &message) {
    if (!check(type)) {
        error(peek(), message);
    }
    return advance();
}

void Parser::error(const Token &token, const std::string &message) const {
    std::ostringstream out;
    out << token.location.filename << ':' << token.location.line << ':' << token.location.column
        << ": error: " << message << " (got " << tokenTypeName(token.type);
    if (!token.lexeme.empty()) {
        out << " '" << token.lexeme << "'";
    }
    out << ")";
    throw std::runtime_error(out.str());
}

bool Parser::isTypeToken(TokenType type) const {
    return type == TokenType::Int || type == TokenType::Float || type == TokenType::Char ||
           type == TokenType::Void;
}

bool Parser::startsType(TokenType type) const {
    return isTypeToken(type) || type == TokenType::Struct || type == TokenType::Union || type == TokenType::Enum;
}

Type Parser::tokenToType(TokenType type) const {
    switch (type) {
    case TokenType::Int: return Type::Int;
    case TokenType::Float: return Type::Float;
    case TokenType::Char: return Type::Char;
    case TokenType::Void: return Type::Void;
    default:
        throw std::logic_error("tokenToType: token is not a type keyword");
    }
}

Type Parser::parseType() {
    const Token &headTok = peek();
    Type base;
    if (headTok.type == TokenType::Struct || headTok.type == TokenType::Union) {
        advance();
        const Token &nameTok = expect(TokenType::Identifier, "expected struct/union tag name");
        base = headTok.type == TokenType::Struct ? Type::namedStruct(nameTok.lexeme) : Type::namedUnion(nameTok.lexeme);
    } else if (headTok.type == TokenType::Enum) {
        advance();
        expect(TokenType::Identifier, "expected enum tag name");
        // Enum constants are plain ints (see Type's doc comment in ast.h),
        // so an `enum Name` type reference is just `int`.
        base = Type::Int;
    } else if (isTypeToken(headTok.type)) {
        advance();
        base = tokenToType(headTok.type);
    } else {
        error(headTok, "expected a type");
    }

    while (match(TokenType::Star)) {
        base = base.pointerTo();
    }
    return base;
}

Type Parser::parseArraySuffix(Type base) {
    if (match(TokenType::LeftBracket)) {
        const Token &sizeTok = expect(TokenType::IntLiteral, "expected array size");
        expect(TokenType::RightBracket, "expected ']' after array size");
        const long long size = std::stoll(sizeTok.lexeme);
        // A Type's arrayLength of 0 means "not an array" (see Type::isArray
        // in ast.h), so a literal size of 0 can't be represented as an
        // array type at all — reject it here rather than silently parsing
        // `int a[0];` as the scalar declaration `int a;`.
        if (size <= 0) {
            error(sizeTok, "array size must be a positive integer");
        }
        return base.arrayOf(static_cast<int>(size));
    }
    return base;
}

// ---------------------------------------------------------------------------
// Top-level
// ---------------------------------------------------------------------------

ProgramNode Parser::parseProgram() {
    ProgramNode program;
    while (!check(TokenType::EndOfFile)) {
        // A leading struct/union/enum keyword is only the start of a new
        // *definition* when it's immediately followed by "tag {" — e.g.
        // `struct Point makeOrigin() { ... }` is a function whose return
        // type happens to reference a struct, not a struct definition, so
        // this needs a 3rd token of lookahead beyond the dispatch tier 1/2
        // ever needed.
        const bool isDefinition = peek(2).type == TokenType::LeftBrace;
        if (check(TokenType::Struct) && isDefinition) {
            program.aggregates.push_back(parseAggregateDecl(/*isUnion=*/false));
        } else if (check(TokenType::Union) && isDefinition) {
            program.aggregates.push_back(parseAggregateDecl(/*isUnion=*/true));
        } else if (check(TokenType::Enum) && isDefinition) {
            program.enums.push_back(parseEnumDecl());
        } else {
            program.functions.push_back(parseFuncDef());
        }
    }
    return program;
}

std::unique_ptr<FuncDefNode> Parser::parseFuncDef() {
    const Token &typeTok = peek();
    Type returnType = parseType();

    const Token &nameTok = expect(TokenType::Identifier, "expected function name");
    expect(TokenType::LeftParen, "expected '(' after function name");

    std::vector<ParamNode> params;
    if (!check(TokenType::RightParen)) {
        params.push_back(parseParam());
        while (match(TokenType::Comma)) {
            params.push_back(parseParam());
        }
    }
    expect(TokenType::RightParen, "expected ')' after parameter list");

    auto body = parseBlock();
    return std::make_unique<FuncDefNode>(typeTok.location, returnType, nameTok.lexeme,
                                          std::move(params), std::move(body));
}

ParamNode Parser::parseParam() {
    const Token &typeTok = peek();
    Type type = parseType();
    const Token &nameTok = expect(TokenType::Identifier, "expected parameter name");
    return ParamNode{type, nameTok.lexeme, typeTok.location};
}

std::unique_ptr<AggregateDeclNode> Parser::parseAggregateDecl(bool isUnion) {
    const Token &headTok = isUnion ? expect(TokenType::Union, "expected 'union'")
                                    : expect(TokenType::Struct, "expected 'struct'");
    const Token &nameTok = expect(TokenType::Identifier, "expected tag name");
    expect(TokenType::LeftBrace, "expected '{' after tag name");

    std::vector<FieldNode> fields;
    while (!check(TokenType::RightBrace) && !check(TokenType::EndOfFile)) {
        fields.push_back(parseFieldDecl());
    }
    expect(TokenType::RightBrace, "expected '}' after field list");
    expect(TokenType::Semicolon, "expected ';' after struct/union declaration");

    return std::make_unique<AggregateDeclNode>(headTok.location, nameTok.lexeme, std::move(fields), isUnion);
}

FieldNode Parser::parseFieldDecl() {
    const Token &typeTok = peek();
    Type type = parseType();
    const Token &nameTok = expect(TokenType::Identifier, "expected field name");
    type = parseArraySuffix(type);
    expect(TokenType::Semicolon, "expected ';' after field declaration");
    return FieldNode{type, nameTok.lexeme, typeTok.location};
}

std::unique_ptr<EnumDeclNode> Parser::parseEnumDecl() {
    const Token &enumTok = expect(TokenType::Enum, "expected 'enum'");
    const Token &nameTok = expect(TokenType::Identifier, "expected enum tag name");
    expect(TokenType::LeftBrace, "expected '{' after enum tag name");

    std::vector<EnumeratorNode> enumerators;
    long long nextValue = 0;
    if (!check(TokenType::RightBrace)) {
        enumerators.push_back(parseEnumerator(nextValue));
        while (match(TokenType::Comma)) {
            enumerators.push_back(parseEnumerator(nextValue));
        }
    }
    expect(TokenType::RightBrace, "expected '}' after enumerator list");
    expect(TokenType::Semicolon, "expected ';' after enum declaration");

    return std::make_unique<EnumDeclNode>(enumTok.location, nameTok.lexeme, std::move(enumerators));
}

EnumeratorNode Parser::parseEnumerator(long long &nextValue) {
    const Token &nameTok = expect(TokenType::Identifier, "expected enumerator name");
    long long value = nextValue;
    if (match(TokenType::Assign)) {
        const Token &valueTok = expect(TokenType::IntLiteral, "expected integer constant");
        value = std::stoll(valueTok.lexeme);
    }
    nextValue = value + 1;
    return EnumeratorNode{nameTok.lexeme, value, nameTok.location};
}

// ---------------------------------------------------------------------------
// Statements
// ---------------------------------------------------------------------------

std::unique_ptr<BlockStmtNode> Parser::parseBlock() {
    const Token &braceTok = expect(TokenType::LeftBrace, "expected '{'");
    std::vector<StmtPtr> statements;
    while (!check(TokenType::RightBrace) && !check(TokenType::EndOfFile)) {
        statements.push_back(parseStatement());
    }
    expect(TokenType::RightBrace, "expected '}'");
    return std::make_unique<BlockStmtNode>(braceTok.location, std::move(statements));
}

StmtPtr Parser::parseStatement() {
    switch (peek().type) {
    case TokenType::Int:
    case TokenType::Float:
    case TokenType::Char:
    case TokenType::Void:
    case TokenType::Struct:
    case TokenType::Union:
    case TokenType::Enum:
        return parseVarDecl();
    case TokenType::If:
        return parseIf();
    case TokenType::While:
        return parseWhile();
    case TokenType::For:
        return parseFor();
    case TokenType::Return:
        return parseReturn();
    case TokenType::Break:
        return parseBreak();
    case TokenType::Continue:
        return parseContinue();
    case TokenType::Do:
        return parseDoWhile();
    case TokenType::Switch:
        return parseSwitch();
    case TokenType::Case:
        return parseCaseLabel();
    case TokenType::Default:
        return parseDefaultLabel();
    case TokenType::Goto:
        return parseGoto();
    case TokenType::Identifier:
        // `name:` is a goto label; anything else starting with an
        // identifier is an assignment or call/inc-dec expression statement.
        if (peek(1).type == TokenType::Colon) {
            return parseLabel();
        }
        return parseAssignOrExprStmt();
    case TokenType::Star:
    case TokenType::PlusPlus:
    case TokenType::MinusMinus:
        return parseAssignOrExprStmt();
    default:
        error(peek(), "expected a statement");
    }
}

std::unique_ptr<VarDeclStmtNode> Parser::parseVarDeclNoSemi() {
    const Token &typeTok = peek();
    Type type = parseType();
    const Token &nameTok = expect(TokenType::Identifier, "expected variable name");
    type = parseArraySuffix(type);

    ExprPtr init;
    if (match(TokenType::Assign)) {
        init = parseExpression();
    }
    return std::make_unique<VarDeclStmtNode>(typeTok.location, type, nameTok.lexeme, std::move(init));
}

StmtPtr Parser::parseVarDecl() {
    auto decl = parseVarDeclNoSemi();
    expect(TokenType::Semicolon, "expected ';' after variable declaration");
    return decl;
}

bool Parser::isCompoundAssignToken(TokenType type) const {
    switch (type) {
    case TokenType::PlusAssign:
    case TokenType::MinusAssign:
    case TokenType::StarAssign:
    case TokenType::SlashAssign:
    case TokenType::AmpAssign:
    case TokenType::PipeAssign:
    case TokenType::CaretAssign:
    case TokenType::ShlAssign:
    case TokenType::ShrAssign:
        return true;
    default:
        return false;
    }
}

BinaryOp Parser::compoundAssignOp(TokenType type) const {
    switch (type) {
    case TokenType::PlusAssign: return BinaryOp::Add;
    case TokenType::MinusAssign: return BinaryOp::Sub;
    case TokenType::StarAssign: return BinaryOp::Mul;
    case TokenType::SlashAssign: return BinaryOp::Div;
    case TokenType::AmpAssign: return BinaryOp::BitAnd;
    case TokenType::PipeAssign: return BinaryOp::BitOr;
    case TokenType::CaretAssign: return BinaryOp::BitXor;
    case TokenType::ShlAssign: return BinaryOp::Shl;
    case TokenType::ShrAssign: return BinaryOp::Shr;
    default:
        throw std::logic_error("compoundAssignOp: token is not a compound-assignment operator");
    }
}

StmtPtr Parser::parseSimpleStmtNoSemi() {
    const Token &startTok = peek();
    ExprPtr expr = parseUnary();

    if (check(TokenType::Assign)) {
        advance();
        ExprPtr value = parseExpression();
        return std::make_unique<AssignStmtNode>(startTok.location, std::move(expr), std::move(value));
    }
    if (isCompoundAssignToken(peek().type)) {
        const Token &opTok = advance();
        ExprPtr value = parseExpression();
        return std::make_unique<AssignStmtNode>(startTok.location, std::move(expr), compoundAssignOp(opTok.type),
                                                std::move(value));
    }

    return std::make_unique<ExprStmtNode>(startTok.location, std::move(expr));
}

StmtPtr Parser::parseAssignOrExprStmt() {
    StmtPtr stmt = parseSimpleStmtNoSemi();
    // A bare expression statement only makes sense if it has a side effect
    // (a call, or ++/--); anything else (e.g. a stray `1 + 2;`) is silently
    // pointless and almost always a typo for an assignment.
    if (const auto *exprStmt = dynamic_cast<const ExprStmtNode *>(stmt.get())) {
        if (!dynamic_cast<const CallExprNode *>(exprStmt->expr.get()) &&
            !dynamic_cast<const IncDecExprNode *>(exprStmt->expr.get())) {
            error(peek(), "expected '=' or '(' after expression");
        }
    }
    expect(TokenType::Semicolon, "expected ';' after statement");
    return stmt;
}

StmtPtr Parser::parseIf() {
    const Token &ifTok = expect(TokenType::If, "expected 'if'");
    expect(TokenType::LeftParen, "expected '(' after 'if'");
    ExprPtr condition = parseExpression();
    expect(TokenType::RightParen, "expected ')' after condition");

    auto thenBlock = parseBlock();

    std::unique_ptr<BlockStmtNode> elseBlock;
    if (match(TokenType::Else)) {
        if (check(TokenType::If)) {
            // Treat "else if" as an else-block containing a single nested if.
            const SourceLocation loc = peek().location;
            std::vector<StmtPtr> stmts;
            stmts.push_back(parseIf());
            elseBlock = std::make_unique<BlockStmtNode>(loc, std::move(stmts));
        } else {
            elseBlock = parseBlock();
        }
    }

    return std::make_unique<IfStmtNode>(ifTok.location, std::move(condition), std::move(thenBlock),
                                         std::move(elseBlock));
}

StmtPtr Parser::parseWhile() {
    const Token &whileTok = expect(TokenType::While, "expected 'while'");
    expect(TokenType::LeftParen, "expected '(' after 'while'");
    ExprPtr condition = parseExpression();
    expect(TokenType::RightParen, "expected ')' after condition");
    auto body = parseBlock();
    return std::make_unique<WhileStmtNode>(whileTok.location, std::move(condition), std::move(body));
}

StmtPtr Parser::parseFor() {
    const Token &forTok = expect(TokenType::For, "expected 'for'");
    expect(TokenType::LeftParen, "expected '(' after 'for'");

    StmtPtr init;
    if (!check(TokenType::Semicolon)) {
        init = parseForInit();
    }
    expect(TokenType::Semicolon, "expected ';' after for-loop initializer");

    ExprPtr condition;
    if (!check(TokenType::Semicolon)) {
        condition = parseExpression();
    }
    expect(TokenType::Semicolon, "expected ';' after for-loop condition");

    StmtPtr update;
    if (!check(TokenType::RightParen)) {
        update = parseForUpdate();
    }
    expect(TokenType::RightParen, "expected ')' after for-loop update");

    auto body = parseBlock();
    return std::make_unique<ForStmtNode>(forTok.location, std::move(init), std::move(condition),
                                          std::move(update), std::move(body));
}

StmtPtr Parser::parseForInit() {
    if (startsType(peek().type)) {
        return parseVarDeclNoSemi();
    }
    return parseSimpleStmtNoSemi();
}

StmtPtr Parser::parseForUpdate() {
    return parseSimpleStmtNoSemi();
}

StmtPtr Parser::parseReturn() {
    const Token &returnTok = expect(TokenType::Return, "expected 'return'");
    ExprPtr value;
    if (!check(TokenType::Semicolon)) {
        value = parseExpression();
    }
    expect(TokenType::Semicolon, "expected ';' after return statement");
    return std::make_unique<ReturnStmtNode>(returnTok.location, std::move(value));
}

StmtPtr Parser::parseBreak() {
    const Token &breakTok = expect(TokenType::Break, "expected 'break'");
    expect(TokenType::Semicolon, "expected ';' after 'break'");
    return std::make_unique<BreakStmtNode>(breakTok.location);
}

StmtPtr Parser::parseContinue() {
    const Token &continueTok = expect(TokenType::Continue, "expected 'continue'");
    expect(TokenType::Semicolon, "expected ';' after 'continue'");
    return std::make_unique<ContinueStmtNode>(continueTok.location);
}

StmtPtr Parser::parseDoWhile() {
    const Token &doTok = expect(TokenType::Do, "expected 'do'");
    auto body = parseBlock();
    expect(TokenType::While, "expected 'while' after 'do' block");
    expect(TokenType::LeftParen, "expected '(' after 'while'");
    ExprPtr condition = parseExpression();
    expect(TokenType::RightParen, "expected ')' after condition");
    expect(TokenType::Semicolon, "expected ';' after do-while statement");
    return std::make_unique<DoWhileStmtNode>(doTok.location, std::move(body), std::move(condition));
}

StmtPtr Parser::parseSwitch() {
    const Token &switchTok = expect(TokenType::Switch, "expected 'switch'");
    expect(TokenType::LeftParen, "expected '(' after 'switch'");
    ExprPtr value = parseExpression();
    expect(TokenType::RightParen, "expected ')' after switch value");
    auto body = parseBlock();
    return std::make_unique<SwitchStmtNode>(switchTok.location, std::move(value), std::move(body));
}

StmtPtr Parser::parseCaseLabel() {
    const Token &caseTok = expect(TokenType::Case, "expected 'case'");
    const bool negative = match(TokenType::Minus);
    const Token &valueTok = expect(TokenType::IntLiteral, "expected integer constant after 'case'");
    long long value = std::stoll(valueTok.lexeme);
    expect(TokenType::Colon, "expected ':' after case value");
    return std::make_unique<CaseLabelStmtNode>(caseTok.location, negative ? -value : value);
}

StmtPtr Parser::parseDefaultLabel() {
    const Token &defaultTok = expect(TokenType::Default, "expected 'default'");
    expect(TokenType::Colon, "expected ':' after 'default'");
    return std::make_unique<DefaultLabelStmtNode>(defaultTok.location);
}

StmtPtr Parser::parseGoto() {
    const Token &gotoTok = expect(TokenType::Goto, "expected 'goto'");
    const Token &nameTok = expect(TokenType::Identifier, "expected label name after 'goto'");
    expect(TokenType::Semicolon, "expected ';' after 'goto' statement");
    return std::make_unique<GotoStmtNode>(gotoTok.location, nameTok.lexeme);
}

StmtPtr Parser::parseLabel() {
    const Token &nameTok = expect(TokenType::Identifier, "expected label name");
    expect(TokenType::Colon, "expected ':' after label name");
    return std::make_unique<LabelStmtNode>(nameTok.location, nameTok.lexeme);
}

// ---------------------------------------------------------------------------
// Expressions
//
// Precedence, lowest to highest: ?: , || , && , | , ^ , & , == != ,
// < > <= >= , << >> , + - , * / , unary ! - & * ~ ++ -- (address-of /
// deref / bitnot / pre-inc/dec), postfix [] . -> ++ -- (indexing / member
// access / post-inc/dec), primary.
// ---------------------------------------------------------------------------

ExprPtr Parser::parseExpression() {
    return parseTernary();
}

ExprPtr Parser::parseTernary() {
    ExprPtr condition = parseLogicalOr();
    if (!check(TokenType::Question)) {
        return condition;
    }
    const Token &questionTok = advance();
    ExprPtr thenExpr = parseExpression();
    expect(TokenType::Colon, "expected ':' in ternary expression");
    ExprPtr elseExpr = parseTernary();
    return std::make_unique<TernaryExprNode>(questionTok.location, std::move(condition), std::move(thenExpr),
                                             std::move(elseExpr));
}

ExprPtr Parser::parseLogicalOr() {
    ExprPtr left = parseLogicalAnd();
    while (check(TokenType::Or)) {
        const Token &opTok = advance();
        ExprPtr right = parseLogicalAnd();
        left = std::make_unique<BinOpExprNode>(opTok.location, BinaryOp::Or, std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::parseLogicalAnd() {
    ExprPtr left = parseBitwiseOr();
    while (check(TokenType::And)) {
        const Token &opTok = advance();
        ExprPtr right = parseBitwiseOr();
        left = std::make_unique<BinOpExprNode>(opTok.location, BinaryOp::And, std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::parseBitwiseOr() {
    ExprPtr left = parseBitwiseXor();
    while (check(TokenType::Pipe)) {
        const Token &opTok = advance();
        ExprPtr right = parseBitwiseXor();
        left = std::make_unique<BinOpExprNode>(opTok.location, BinaryOp::BitOr, std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::parseBitwiseXor() {
    ExprPtr left = parseBitwiseAnd();
    while (check(TokenType::Caret)) {
        const Token &opTok = advance();
        ExprPtr right = parseBitwiseAnd();
        left = std::make_unique<BinOpExprNode>(opTok.location, BinaryOp::BitXor, std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::parseBitwiseAnd() {
    ExprPtr left = parseEquality();
    while (check(TokenType::Ampersand)) {
        const Token &opTok = advance();
        ExprPtr right = parseEquality();
        left = std::make_unique<BinOpExprNode>(opTok.location, BinaryOp::BitAnd, std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::parseEquality() {
    ExprPtr left = parseComparison();
    while (check(TokenType::Equal) || check(TokenType::NotEqual)) {
        const Token &opTok = advance();
        const BinaryOp op = opTok.type == TokenType::Equal ? BinaryOp::Eq : BinaryOp::Neq;
        ExprPtr right = parseComparison();
        left = std::make_unique<BinOpExprNode>(opTok.location, op, std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::parseComparison() {
    ExprPtr left = parseShift();
    while (check(TokenType::Less) || check(TokenType::Greater) || check(TokenType::LessEqual) ||
           check(TokenType::GreaterEqual)) {
        const Token &opTok = advance();
        BinaryOp op;
        switch (opTok.type) {
        case TokenType::Less: op = BinaryOp::Lt; break;
        case TokenType::Greater: op = BinaryOp::Gt; break;
        case TokenType::LessEqual: op = BinaryOp::Leq; break;
        default: op = BinaryOp::Geq; break;
        }
        ExprPtr right = parseShift();
        left = std::make_unique<BinOpExprNode>(opTok.location, op, std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::parseShift() {
    ExprPtr left = parseAdditive();
    while (check(TokenType::LeftShift) || check(TokenType::RightShift)) {
        const Token &opTok = advance();
        const BinaryOp op = opTok.type == TokenType::LeftShift ? BinaryOp::Shl : BinaryOp::Shr;
        ExprPtr right = parseAdditive();
        left = std::make_unique<BinOpExprNode>(opTok.location, op, std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::parseAdditive() {
    ExprPtr left = parseMultiplicative();
    while (check(TokenType::Plus) || check(TokenType::Minus)) {
        const Token &opTok = advance();
        const BinaryOp op = opTok.type == TokenType::Plus ? BinaryOp::Add : BinaryOp::Sub;
        ExprPtr right = parseMultiplicative();
        left = std::make_unique<BinOpExprNode>(opTok.location, op, std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::parseMultiplicative() {
    ExprPtr left = parseUnary();
    while (check(TokenType::Star) || check(TokenType::Slash)) {
        const Token &opTok = advance();
        const BinaryOp op = opTok.type == TokenType::Star ? BinaryOp::Mul : BinaryOp::Div;
        ExprPtr right = parseUnary();
        left = std::make_unique<BinOpExprNode>(opTok.location, op, std::move(left), std::move(right));
    }
    return left;
}

ExprPtr Parser::parseUnary() {
    if (check(TokenType::Not) || check(TokenType::Minus) || check(TokenType::Tilde)) {
        const Token &opTok = advance();
        UnaryOp op = UnaryOp::Not;
        if (opTok.type == TokenType::Minus) {
            op = UnaryOp::Negate;
        } else if (opTok.type == TokenType::Tilde) {
            op = UnaryOp::BitNot;
        }
        ExprPtr operand = parseUnary();
        return std::make_unique<UnaryOpExprNode>(opTok.location, op, std::move(operand));
    }
    if (check(TokenType::Ampersand) || check(TokenType::Star)) {
        const Token &opTok = advance();
        const UnaryOp op = opTok.type == TokenType::Ampersand ? UnaryOp::AddressOf : UnaryOp::Deref;
        ExprPtr operand = parseUnary();
        return std::make_unique<UnaryOpExprNode>(opTok.location, op, std::move(operand));
    }
    if (check(TokenType::PlusPlus) || check(TokenType::MinusMinus)) {
        const Token &opTok = advance();
        ExprPtr target = parseUnary();
        return std::make_unique<IncDecExprNode>(opTok.location, std::move(target),
                                                /*isIncrement=*/opTok.type == TokenType::PlusPlus,
                                                /*isPrefix=*/true);
    }
    return parsePostfix();
}

ExprPtr Parser::parsePostfix() {
    ExprPtr expr = parsePrimary();
    while (true) {
        if (check(TokenType::LeftBracket)) {
            const Token &bracketTok = advance();
            ExprPtr index = parseExpression();
            expect(TokenType::RightBracket, "expected ']' after array index");
            expr = std::make_unique<IndexExprNode>(bracketTok.location, std::move(expr), std::move(index));
        } else if (check(TokenType::Dot)) {
            const Token &dotTok = advance();
            const Token &fieldTok = expect(TokenType::Identifier, "expected field name after '.'");
            expr = std::make_unique<MemberExprNode>(dotTok.location, std::move(expr), fieldTok.lexeme);
        } else if (check(TokenType::Arrow)) {
            const Token &arrowTok = advance();
            const Token &fieldTok = expect(TokenType::Identifier, "expected field name after '->'");
            // Desugar `p->field` to `(*p).field` — see MemberExprNode's
            // doc comment in ast.h.
            auto deref = std::make_unique<UnaryOpExprNode>(arrowTok.location, UnaryOp::Deref, std::move(expr));
            expr = std::make_unique<MemberExprNode>(arrowTok.location, std::move(deref), fieldTok.lexeme);
        } else if (check(TokenType::PlusPlus) || check(TokenType::MinusMinus)) {
            const Token &opTok = advance();
            expr = std::make_unique<IncDecExprNode>(opTok.location, std::move(expr),
                                                    /*isIncrement=*/opTok.type == TokenType::PlusPlus,
                                                    /*isPrefix=*/false);
        } else {
            break;
        }
    }
    return expr;
}

ExprPtr Parser::parsePrimary() {
    const Token &tok = peek();
    switch (tok.type) {
    case TokenType::IntLiteral:
        advance();
        return std::make_unique<IntLitExprNode>(tok.location, std::stoll(tok.lexeme));
    case TokenType::FloatLiteral:
        advance();
        return std::make_unique<FloatLitExprNode>(tok.location, std::stod(tok.lexeme));
    case TokenType::CharLiteral:
        advance();
        return std::make_unique<CharLitExprNode>(tok.location, decodeCharLiteral(tok.lexeme));
    case TokenType::StringLiteral:
        advance();
        return std::make_unique<StringLitExprNode>(tok.location, decodeStringLiteral(tok.lexeme));
    case TokenType::Identifier: {
        advance();
        if (check(TokenType::LeftParen)) {
            return parseCallExpr(tok);
        }
        return std::make_unique<IdentExprNode>(tok.location, tok.lexeme);
    }
    case TokenType::LeftParen: {
        advance();
        ExprPtr expr = parseExpression();
        // The comma operator is only reachable inside explicit parens, so
        // it can never be confused with a comma-separated argument or
        // parameter list.
        while (check(TokenType::Comma)) {
            const Token &commaTok = advance();
            ExprPtr rhs = parseExpression();
            expr = std::make_unique<BinOpExprNode>(commaTok.location, BinaryOp::Comma, std::move(expr),
                                                   std::move(rhs));
        }
        expect(TokenType::RightParen, "expected ')' after expression");
        return expr;
    }
    default:
        error(tok, "expected an expression");
    }
}

ExprPtr Parser::parseCallExpr(const Token &calleeTok) {
    expect(TokenType::LeftParen, "expected '(' in call expression");
    std::vector<ExprPtr> args;
    if (!check(TokenType::RightParen)) {
        args.push_back(parseExpression());
        while (match(TokenType::Comma)) {
            args.push_back(parseExpression());
        }
    }
    expect(TokenType::RightParen, "expected ')' after argument list");
    return std::make_unique<CallExprNode>(calleeTok.location, calleeTok.lexeme, std::move(args));
}

} // namespace minic
