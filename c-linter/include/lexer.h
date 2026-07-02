#ifndef CL_LEXER_H
#define CL_LEXER_H

#include "token.h"

#include <cstddef>
#include <string>
#include <vector>

namespace cl {

// Tolerant by design: unlike a compiler's lexer, this must never throw.
// Malformed/unterminated comments, strings, and char literals just consume
// to end-of-input rather than erroring, and any punctuation the linter has
// no rule for becomes a single-character Other token (see token.h). A
// linter has to process real-world C it doesn't fully model, not reject it.
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
    Token lexStringLiteral(int startLine, int startColumn);
    Token lexCharLiteral(int startLine, int startColumn);

    std::string source_;
    std::string filename_;
    std::size_t pos_ = 0;
    int line_ = 1;
    int column_ = 1;
};

} // namespace cl

#endif // CL_LEXER_H
