#pragma once

#include <ostream>
#include <string>

namespace minic {

enum class TokenType {
    Int,
    Float,
    Char,
    Void,
    If,
    Else,
    While,
    For,
    Return,
    Break,
    Continue,
    Struct,
    Union,
    Enum,
    Do,
    Switch,
    Case,
    Default,
    Goto,
    Sizeof,
    Const,
    Static,
    Extern,
    Volatile,
    Identifier,
    IntLiteral,
    FloatLiteral,
    CharLiteral,
    StringLiteral,
    Plus,
    Minus,
    Star,
    Slash,
    Equal,
    NotEqual,
    Less,
    Greater,
    LessEqual,
    GreaterEqual,
    And,
    Or,
    Not,
    Ampersand,
    Pipe,
    Caret,
    Tilde,
    LeftShift,
    RightShift,
    Question,
    Colon,
    PlusPlus,
    MinusMinus,
    PlusAssign,
    MinusAssign,
    StarAssign,
    SlashAssign,
    AmpAssign,
    PipeAssign,
    CaretAssign,
    ShlAssign,
    ShrAssign,
    Assign,
    LeftParen,
    RightParen,
    LeftBrace,
    RightBrace,
    LeftBracket,
    RightBracket,
    Semicolon,
    Comma,
    Dot,
    Arrow,
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
std::ostream &operator<<(std::ostream &out, const Token &token);

} // namespace minic
