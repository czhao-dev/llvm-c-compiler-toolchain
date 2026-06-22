#ifndef PP_TOKENIZER_H
#define PP_TOKENIZER_H

#include <string>
#include <vector>

#include "token.h"

namespace pp {

// Tokenizes a single already-comment-stripped line (or a #define
// replacement-text fragment) into a flat stream whose token texts, when
// concatenated in order, reproduce the input exactly. Used to find
// identifier boundaries for macro expansion while skipping over
// string/char literal contents (never scanned for macro names inside).
class PPTokenizer {
public:
    explicit PPTokenizer(std::string text);

    std::vector<PPToken> tokenize();

private:
    PPToken lexIdentifier();
    PPToken lexNumber();
    PPToken lexQuoted(char quote, PPTokenKind kind);
    PPToken lexWhitespace();
    PPToken lexPunct();

    std::string text_;
    std::size_t pos_ = 0;
};

} // namespace pp

#endif // PP_TOKENIZER_H
