#ifndef CL_TOKEN_H
#define CL_TOKEN_H

#include <string>

namespace cl {

// Deliberately minimal: `if`/`while` are the only keywords any rule needs
// to distinguish from a plain identifier (see brace_style_rule.h). Every
// other keyword-shaped word (int, return, struct, sizeof, ...) lexes as
// Identifier — safe because no rule needs finer keyword granularity, and
// real C keywords are always lowercase so the naming rule can never flag
// one. `Other` is a catch-all for punctuation/operators the linter has no
// rule for (see lexer.h) so it can tokenize arbitrary real C without ever
// erroring.
enum class TokenType {
    Identifier,
    IntLiteral,
    FloatLiteral,
    StringLiteral,
    CharLiteral,
    If,
    While,
    EqualEqual,
    NotEqual,
    Less,
    Greater,
    LessEqual,
    GreaterEqual,
    LeftParen,
    RightParen,
    LeftBrace,
    RightBrace,
    Other,
    EndOfFile,
};

struct SourceLocation {
    std::string filename;
    int line = 1;
    int column = 1;
};

struct Token {
    TokenType type;
    std::string lexeme;
    SourceLocation location;
};

std::string tokenTypeName(TokenType type);

} // namespace cl

#endif // CL_TOKEN_H
