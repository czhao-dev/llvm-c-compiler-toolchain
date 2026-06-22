#include "pp_tokenizer.h"

#include <cctype>

#include "diagnostics.h"

namespace pp {

bool isIdentifierStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) != 0 || c == '_';
}

bool isIdentifierContinue(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

PPTokenizer::PPTokenizer(std::string text) : text_(std::move(text)) {}

std::vector<PPToken> PPTokenizer::tokenize() {
    std::vector<PPToken> out;
    while (pos_ < text_.size()) {
        char c = text_[pos_];
        if (isIdentifierStart(c)) {
            out.push_back(lexIdentifier());
        } else if (std::isdigit(static_cast<unsigned char>(c)) != 0) {
            out.push_back(lexNumber());
        } else if (c == '"') {
            out.push_back(lexQuoted('"', PPTokenKind::StringLiteral));
        } else if (c == '\'') {
            out.push_back(lexQuoted('\'', PPTokenKind::CharLiteral));
        } else if (c == ' ' || c == '\t') {
            out.push_back(lexWhitespace());
        } else {
            out.push_back(lexPunct());
        }
    }
    return out;
}

PPToken PPTokenizer::lexIdentifier() {
    std::size_t start = pos_;
    while (pos_ < text_.size() && isIdentifierContinue(text_[pos_])) {
        pos_ += 1;
    }
    return PPToken{PPTokenKind::Identifier, text_.substr(start, pos_ - start), {}};
}

// Deliberately permissive "pp-number" rule: a digit followed by any run of
// letters/digits/'_'/'.'. This over-matches real C numeric-literal grammar
// on purpose so a numeric literal like 0X10 is always one Number token and
// never fractures into pieces that could accidentally collide with a macro
// name (e.g. a macro literally named X must never match inside 0X10).
PPToken PPTokenizer::lexNumber() {
    std::size_t start = pos_;
    while (pos_ < text_.size()) {
        char c = text_[pos_];
        if (std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '.') {
            pos_ += 1;
        } else {
            break;
        }
    }
    return PPToken{PPTokenKind::Number, text_.substr(start, pos_ - start), {}};
}

PPToken PPTokenizer::lexQuoted(char quote, PPTokenKind kind) {
    std::size_t start = pos_;
    pos_ += 1; // consume opening quote
    while (pos_ < text_.size()) {
        char c = text_[pos_];
        if (c == '\\' && pos_ + 1 < text_.size()) {
            pos_ += 2;
        } else if (c == quote) {
            pos_ += 1;
            return PPToken{kind, text_.substr(start, pos_ - start), {}};
        } else {
            pos_ += 1;
        }
    }
    // Defensive: CommentStripper already validates literal termination for
    // the whole file, so this should be unreachable in normal use — it's
    // only reachable when PPTokenizer is fed a fragment directly (e.g. a
    // #define replacement text containing a stray unmatched quote).
    throw PreprocessorError("<macro-text>", 0, "unterminated string/character literal");
}

PPToken PPTokenizer::lexWhitespace() {
    std::size_t start = pos_;
    while (pos_ < text_.size() && (text_[pos_] == ' ' || text_[pos_] == '\t')) {
        pos_ += 1;
    }
    return PPToken{PPTokenKind::Whitespace, text_.substr(start, pos_ - start), {}};
}

PPToken PPTokenizer::lexPunct() {
    std::size_t start = pos_;
    pos_ += 1;
    return PPToken{PPTokenKind::Punct, text_.substr(start, pos_ - start), {}};
}

} // namespace pp
