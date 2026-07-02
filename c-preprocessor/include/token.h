#ifndef PP_TOKEN_H
#define PP_TOKEN_H

#include <string>
#include <unordered_set>

namespace pp {

enum class PPTokenKind {
    Identifier,
    Number,
    StringLiteral,
    CharLiteral,
    Whitespace,
    Punct,
};

// The set of macro names that must not be re-expanded when this token is
// rescanned ("blue paint"). Only meaningful when kind == Identifier.
using HideSet = std::unordered_set<std::string>;

struct PPToken {
    PPTokenKind kind;
    std::string text;  // exact source text; concatenating every token's
                        // text in order reproduces the input exactly
    HideSet hideSet;
};

bool isIdentifierStart(char c);
bool isIdentifierContinue(char c);

} // namespace pp

#endif // PP_TOKEN_H
