#include "lexer.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

namespace {

void expectTypes(const std::string &source, const std::vector<minic::TokenType> &expected) {
    minic::Lexer lexer(source);
    const auto tokens = lexer.tokenize();
    assert(tokens.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        if (tokens[i].type != expected[i]) {
            std::cerr << "token " << i << ": expected "
                      << minic::tokenTypeName(expected[i]) << ", got "
                      << minic::tokenTypeName(tokens[i].type) << '\n';
            std::abort();
        }
    }
}

} // namespace

int main() {
    using minic::TokenType;

    expectTypes(
        "int main() { int x = 5; x = x + 1; return x; }",
        {
            TokenType::Int,
            TokenType::Identifier,
            TokenType::LeftParen,
            TokenType::RightParen,
            TokenType::LeftBrace,
            TokenType::Int,
            TokenType::Identifier,
            TokenType::Assign,
            TokenType::IntLiteral,
            TokenType::Semicolon,
            TokenType::Identifier,
            TokenType::Assign,
            TokenType::Identifier,
            TokenType::Plus,
            TokenType::IntLiteral,
            TokenType::Semicolon,
            TokenType::Return,
            TokenType::Identifier,
            TokenType::Semicolon,
            TokenType::RightBrace,
            TokenType::EndOfFile,
        });

    expectTypes(
        "if (n <= 1 || n != 3 && !done) { printf(\"%d\\n\", n); }",
        {
            TokenType::If,
            TokenType::LeftParen,
            TokenType::Identifier,
            TokenType::LessEqual,
            TokenType::IntLiteral,
            TokenType::Or,
            TokenType::Identifier,
            TokenType::NotEqual,
            TokenType::IntLiteral,
            TokenType::And,
            TokenType::Not,
            TokenType::Identifier,
            TokenType::RightParen,
            TokenType::LeftBrace,
            TokenType::Identifier,
            TokenType::LeftParen,
            TokenType::StringLiteral,
            TokenType::Comma,
            TokenType::Identifier,
            TokenType::RightParen,
            TokenType::Semicolon,
            TokenType::RightBrace,
            TokenType::EndOfFile,
        });

    expectTypes(
        "char c = '\\n'; float f = 1.5; // comment\nfor (;;) { break; continue; }",
        {
            TokenType::Char,
            TokenType::Identifier,
            TokenType::Assign,
            TokenType::CharLiteral,
            TokenType::Semicolon,
            TokenType::Float,
            TokenType::Identifier,
            TokenType::Assign,
            TokenType::FloatLiteral,
            TokenType::Semicolon,
            TokenType::For,
            TokenType::LeftParen,
            TokenType::Semicolon,
            TokenType::Semicolon,
            TokenType::RightParen,
            TokenType::LeftBrace,
            TokenType::Break,
            TokenType::Semicolon,
            TokenType::Continue,
            TokenType::Semicolon,
            TokenType::RightBrace,
            TokenType::EndOfFile,
        });

    expectTypes(
        "int arr[5]; arr[0] = 1;",
        {
            TokenType::Int,
            TokenType::Identifier,
            TokenType::LeftBracket,
            TokenType::IntLiteral,
            TokenType::RightBracket,
            TokenType::Semicolon,
            TokenType::Identifier,
            TokenType::LeftBracket,
            TokenType::IntLiteral,
            TokenType::RightBracket,
            TokenType::Assign,
            TokenType::IntLiteral,
            TokenType::Semicolon,
            TokenType::EndOfFile,
        });

    expectTypes(
        "struct Point { int x; }; p.x = 1; p->x;",
        {
            TokenType::Struct,
            TokenType::Identifier,
            TokenType::LeftBrace,
            TokenType::Int,
            TokenType::Identifier,
            TokenType::Semicolon,
            TokenType::RightBrace,
            TokenType::Semicolon,
            TokenType::Identifier,
            TokenType::Dot,
            TokenType::Identifier,
            TokenType::Assign,
            TokenType::IntLiteral,
            TokenType::Semicolon,
            TokenType::Identifier,
            TokenType::Arrow,
            TokenType::Identifier,
            TokenType::Semicolon,
            TokenType::EndOfFile,
        });

    expectTypes(
        "union U {} enum E {}",
        {
            TokenType::Union,
            TokenType::Identifier,
            TokenType::LeftBrace,
            TokenType::RightBrace,
            TokenType::Enum,
            TokenType::Identifier,
            TokenType::LeftBrace,
            TokenType::RightBrace,
            TokenType::EndOfFile,
        });

    expectTypes(
        "a & b | c ^ ~d << 1 >> 2; a ? b : c; i++; --i; a += 1; a <<= 1;",
        {
            TokenType::Identifier,
            TokenType::Ampersand,
            TokenType::Identifier,
            TokenType::Pipe,
            TokenType::Identifier,
            TokenType::Caret,
            TokenType::Tilde,
            TokenType::Identifier,
            TokenType::LeftShift,
            TokenType::IntLiteral,
            TokenType::RightShift,
            TokenType::IntLiteral,
            TokenType::Semicolon,
            TokenType::Identifier,
            TokenType::Question,
            TokenType::Identifier,
            TokenType::Colon,
            TokenType::Identifier,
            TokenType::Semicolon,
            TokenType::Identifier,
            TokenType::PlusPlus,
            TokenType::Semicolon,
            TokenType::MinusMinus,
            TokenType::Identifier,
            TokenType::Semicolon,
            TokenType::Identifier,
            TokenType::PlusAssign,
            TokenType::IntLiteral,
            TokenType::Semicolon,
            TokenType::Identifier,
            TokenType::ShlAssign,
            TokenType::IntLiteral,
            TokenType::Semicolon,
            TokenType::EndOfFile,
        });

    expectTypes(
        "do { x++; } while (x < 3); switch (x) { case 1: break; default: goto end; } end: ;",
        {
            TokenType::Do,
            TokenType::LeftBrace,
            TokenType::Identifier,
            TokenType::PlusPlus,
            TokenType::Semicolon,
            TokenType::RightBrace,
            TokenType::While,
            TokenType::LeftParen,
            TokenType::Identifier,
            TokenType::Less,
            TokenType::IntLiteral,
            TokenType::RightParen,
            TokenType::Semicolon,
            TokenType::Switch,
            TokenType::LeftParen,
            TokenType::Identifier,
            TokenType::RightParen,
            TokenType::LeftBrace,
            TokenType::Case,
            TokenType::IntLiteral,
            TokenType::Colon,
            TokenType::Break,
            TokenType::Semicolon,
            TokenType::Default,
            TokenType::Colon,
            TokenType::Goto,
            TokenType::Identifier,
            TokenType::Semicolon,
            TokenType::RightBrace,
            TokenType::Identifier,
            TokenType::Colon,
            TokenType::Semicolon,
            TokenType::EndOfFile,
        });

    expectTypes(
        "int *p = &x; int y = *p;",
        {
            TokenType::Int,
            TokenType::Star,
            TokenType::Identifier,
            TokenType::Assign,
            TokenType::Ampersand,
            TokenType::Identifier,
            TokenType::Semicolon,
            TokenType::Int,
            TokenType::Identifier,
            TokenType::Assign,
            TokenType::Star,
            TokenType::Identifier,
            TokenType::Semicolon,
            TokenType::EndOfFile,
        });

    return 0;
}
