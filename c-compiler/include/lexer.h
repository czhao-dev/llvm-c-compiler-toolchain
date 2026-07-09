#pragma once

#include "token.h"

#include <string>
#include <vector>

namespace minic {

class Lexer {
public:
    explicit Lexer(std::string source, std::string filename = "<input>");

    Token nextToken();
    std::vector<Token> tokenize();

private:
    bool isAtEnd() const;
    char peek() const;
    char peekNext() const;
    char advance();
    bool match(char expected);

    void skipWhitespaceAndComments();
    Token makeToken(TokenType type, std::string lexeme, int startLine, int startColumn) const;
    Token lexIdentifierOrKeyword(int startLine, int startColumn);
    Token lexNumber(int startLine, int startColumn);
    Token lexCharLiteral(int startLine, int startColumn);
    Token lexStringLiteral(int startLine, int startColumn);
    [[noreturn]] void error(int line, int column, const std::string &message) const;

    std::string source_;
    std::string filename_;
    std::size_t pos_ = 0;
    int line_ = 1;
    int column_ = 1;
};

} // namespace minic
