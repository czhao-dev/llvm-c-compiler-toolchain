#include "lexer.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using cl::Lexer;
using cl::Token;
using cl::TokenType;

namespace {

std::vector<Token> tokenize(const std::string &source) { return Lexer(source, "test.c").tokenize(); }

void expect(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::abort();
    }
}

} // namespace

int main() {
    // Empty source yields exactly one EndOfFile token.
    {
        auto tokens = tokenize("");
        expect(tokens.size() == 1, "empty source should yield one token");
        expect(tokens[0].type == TokenType::EndOfFile, "empty source token should be EndOfFile");
    }

    // if/while are distinct keyword tokens; everything else keyword-shaped
    // falls through to Identifier, including real C keywords.
    {
        auto tokens = tokenize("if while return int struct sizeof foo");
        expect(tokens[0].type == TokenType::If, "'if' should be If");
        expect(tokens[1].type == TokenType::While, "'while' should be While");
        expect(tokens[2].type == TokenType::Identifier, "'return' should lex as Identifier");
        expect(tokens[3].type == TokenType::Identifier, "'int' should lex as Identifier");
        expect(tokens[4].type == TokenType::Identifier, "'struct' should lex as Identifier");
        expect(tokens[5].type == TokenType::Identifier, "'sizeof' should lex as Identifier");
        expect(tokens[6].type == TokenType::Identifier, "'foo' should lex as Identifier");
    }

    // Integer literal forms: decimal, hex, octal, and u/l suffixes.
    {
        auto tokens = tokenize("42 0x1A 010 5u 100L 7UL");
        for (int i = 0; i < 6; ++i) {
            expect(tokens[i].type == TokenType::IntLiteral,
                   "expected IntLiteral at index " + std::to_string(i));
        }
    }

    // Float literal forms are distinguished from int literals.
    {
        auto tokens = tokenize("3.14 1.0e10 2.5f 1e-3");
        for (int i = 0; i < 4; ++i) {
            expect(tokens[i].type == TokenType::FloatLiteral,
                   "expected FloatLiteral at index " + std::to_string(i));
        }
    }

    // String/char literal round trip; embedded digits/operators are not
    // separately tokenized.
    {
        auto tokens = tokenize("\"a == 1\" 'x'");
        expect(tokens[0].type == TokenType::StringLiteral, "expected StringLiteral");
        expect(tokens[0].lexeme == "\"a == 1\"", "string lexeme should include quotes verbatim");
        expect(tokens[1].type == TokenType::CharLiteral, "expected CharLiteral");
        expect(tokens[2].type == TokenType::EndOfFile,
               "no extra tokens should appear inside string/char literal");
    }

    // Escaped quote inside a string doesn't terminate it early.
    {
        auto tokens = tokenize("\"a \\\"quoted\\\" b\"");
        expect(tokens[0].type == TokenType::StringLiteral, "expected StringLiteral");
        expect(tokens[1].type == TokenType::EndOfFile, "escaped quote should not split the string literal");
    }

    // Comments (// and /* */) are skipped, including multi-line block
    // comments, and line tracking resumes correctly afterward.
    {
        auto tokens = tokenize("int x; // trailing comment\n/* multi\nline */ int y;");
        expect(tokens[0].lexeme == "int" && tokens[0].location.line == 1,
               "first 'int' should be on line 1");
        std::size_t idx = 0;
        for (; idx < tokens.size(); ++idx) {
            if (tokens[idx].lexeme == "y") break;
        }
        expect(idx < tokens.size(), "'y' identifier should be found");
        expect(tokens[idx].location.line == 3, "'y' should be on line 3 after the multi-line comment");
    }

    // Unterminated comment/string/char must never crash -- tokenize()
    // always terminates with exactly one trailing EndOfFile.
    {
        expect(tokenize("/* unterminated").back().type == TokenType::EndOfFile,
               "unterminated block comment should still terminate");
        expect(tokenize("\"unterminated string").back().type == TokenType::EndOfFile,
               "unterminated string should still terminate");
        expect(tokenize("'unterminated char").back().type == TokenType::EndOfFile,
               "unterminated char literal should still terminate");
    }

    // All 6 comparison operators are distinct, and bare '=' isn't confused
    // with its two-char form.
    {
        auto tokens = tokenize("== != <= >= < > =");
        expect(tokens[0].type == TokenType::EqualEqual, "== should be EqualEqual");
        expect(tokens[1].type == TokenType::NotEqual, "!= should be NotEqual");
        expect(tokens[2].type == TokenType::LessEqual, "<= should be LessEqual");
        expect(tokens[3].type == TokenType::GreaterEqual, ">= should be GreaterEqual");
        expect(tokens[4].type == TokenType::Less, "< should be Less");
        expect(tokens[5].type == TokenType::Greater, "> should be Greater");
        expect(tokens[6].type == TokenType::Other, "bare = should be Other");
    }

    // Arbitrary real C punctuation the lexer has no rule for becomes
    // Other, without throwing -- a linter must tolerate syntax it doesn't
    // model rather than reject it.
    {
        auto tokens = tokenize("int *p = arr[i]; p->field &= mask % 2; #define X 1");
        expect(tokens.back().type == TokenType::EndOfFile,
               "kitchen-sink punctuation snippet should tokenize to completion");
    }

    std::cout << "lexer_test: all checks passed\n";
    return 0;
}
