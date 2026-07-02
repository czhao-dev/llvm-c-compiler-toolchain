#include "lexer.h"

#include <cctype>

namespace cl {

Lexer::Lexer(std::string source, std::string filename)
    : source_(std::move(source)), filename_(std::move(filename)) {}

bool Lexer::isAtEnd() const { return pos_ >= source_.size(); }

char Lexer::peek() const { return isAtEnd() ? '\0' : source_[pos_]; }

char Lexer::peekNext() const {
    return (pos_ + 1 >= source_.size()) ? '\0' : source_[pos_ + 1];
}

char Lexer::advance() {
    char c = source_[pos_++];
    if (c == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    return c;
}

bool Lexer::match(char expected) {
    if (isAtEnd() || peek() != expected) return false;
    advance();
    return true;
}

void Lexer::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '/' && peekNext() == '/') {
            while (!isAtEnd() && peek() != '\n') advance();
        } else if (c == '/' && peekNext() == '*') {
            advance();
            advance();
            while (!isAtEnd() && !(peek() == '*' && peekNext() == '/')) advance();
            if (!isAtEnd()) {
                advance();
                advance();
            }
            // Unterminated block comment: loop above already ran to EOF,
            // so we just fall through — no error, per the tolerant policy.
        } else {
            break;
        }
    }
}

Token Lexer::makeToken(TokenType type, std::string lexeme, int startLine, int startColumn) const {
    SourceLocation loc{filename_, startLine, startColumn};
    return Token{type, std::move(lexeme), loc};
}

Token Lexer::lexIdentifierOrKeyword(int startLine, int startColumn) {
    std::size_t start = pos_ - 1;
    while (!isAtEnd() && (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_')) {
        advance();
    }
    std::string lexeme = source_.substr(start, pos_ - start);

    TokenType type = TokenType::Identifier;
    if (lexeme == "if") {
        type = TokenType::If;
    } else if (lexeme == "while") {
        type = TokenType::While;
    }
    return makeToken(type, lexeme, startLine, startColumn);
}

Token Lexer::lexNumber(int startLine, int startColumn) {
    std::size_t start = pos_ - 1;
    bool isFloat = false;

    if (source_[start] == '0' && !isAtEnd() && (peek() == 'x' || peek() == 'X')) {
        // Hex literal: digits only. Hex floats aren't worth supporting for
        // an MVP linter; they'd still lex (as IntLiteral) rather than error.
        advance();
        while (!isAtEnd() && std::isxdigit(static_cast<unsigned char>(peek()))) advance();
    } else {
        while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) advance();

        if (!isAtEnd() && peek() == '.' && std::isdigit(static_cast<unsigned char>(peekNext()))) {
            isFloat = true;
            advance();
            while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) advance();
        }
        if (!isAtEnd() && (peek() == 'e' || peek() == 'E')) {
            isFloat = true;
            advance();
            if (!isAtEnd() && (peek() == '+' || peek() == '-')) advance();
            while (!isAtEnd() && std::isdigit(static_cast<unsigned char>(peek()))) advance();
        }
    }

    // Suffixes: u/U/l/L for ints, f/F forces float (e.g. 1.0f).
    while (!isAtEnd() && (peek() == 'u' || peek() == 'U' || peek() == 'l' || peek() == 'L' ||
                           peek() == 'f' || peek() == 'F')) {
        if (peek() == 'f' || peek() == 'F') isFloat = true;
        advance();
    }

    std::string lexeme = source_.substr(start, pos_ - start);
    return makeToken(isFloat ? TokenType::FloatLiteral : TokenType::IntLiteral, lexeme, startLine,
                      startColumn);
}

Token Lexer::lexStringLiteral(int startLine, int startColumn) {
    std::size_t start = pos_ - 1;
    while (!isAtEnd() && peek() != '"') {
        if (peek() == '\\') {
            advance();
            if (!isAtEnd()) advance();
        } else {
            advance();
        }
    }
    if (!isAtEnd()) advance(); // closing quote; unterminated just reaches EOF
    std::string lexeme = source_.substr(start, pos_ - start);
    return makeToken(TokenType::StringLiteral, lexeme, startLine, startColumn);
}

Token Lexer::lexCharLiteral(int startLine, int startColumn) {
    std::size_t start = pos_ - 1;
    while (!isAtEnd() && peek() != '\'') {
        if (peek() == '\\') {
            advance();
            if (!isAtEnd()) advance();
        } else {
            advance();
        }
    }
    if (!isAtEnd()) advance(); // closing quote; unterminated just reaches EOF
    std::string lexeme = source_.substr(start, pos_ - start);
    return makeToken(TokenType::CharLiteral, lexeme, startLine, startColumn);
}

Token Lexer::nextToken() {
    skipWhitespaceAndComments();

    int startLine = line_;
    int startColumn = column_;

    if (isAtEnd()) return makeToken(TokenType::EndOfFile, "", startLine, startColumn);

    char c = advance();

    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        return lexIdentifierOrKeyword(startLine, startColumn);
    }
    if (std::isdigit(static_cast<unsigned char>(c))) {
        return lexNumber(startLine, startColumn);
    }
    if (c == '"') return lexStringLiteral(startLine, startColumn);
    if (c == '\'') return lexCharLiteral(startLine, startColumn);

    switch (c) {
    case '(': return makeToken(TokenType::LeftParen, "(", startLine, startColumn);
    case ')': return makeToken(TokenType::RightParen, ")", startLine, startColumn);
    case '{': return makeToken(TokenType::LeftBrace, "{", startLine, startColumn);
    case '}': return makeToken(TokenType::RightBrace, "}", startLine, startColumn);
    case '=':
        if (match('=')) return makeToken(TokenType::EqualEqual, "==", startLine, startColumn);
        return makeToken(TokenType::Other, "=", startLine, startColumn);
    case '!':
        if (match('=')) return makeToken(TokenType::NotEqual, "!=", startLine, startColumn);
        return makeToken(TokenType::Other, "!", startLine, startColumn);
    case '<':
        if (match('=')) return makeToken(TokenType::LessEqual, "<=", startLine, startColumn);
        return makeToken(TokenType::Less, "<", startLine, startColumn);
    case '>':
        if (match('=')) return makeToken(TokenType::GreaterEqual, ">=", startLine, startColumn);
        return makeToken(TokenType::Greater, ">", startLine, startColumn);
    default:
        return makeToken(TokenType::Other, std::string(1, c), startLine, startColumn);
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        Token token = nextToken();
        tokens.push_back(token);
        if (token.type == TokenType::EndOfFile) break;
    }
    return tokens;
}

} // namespace cl
