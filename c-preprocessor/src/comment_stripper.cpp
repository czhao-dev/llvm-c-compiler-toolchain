#include "comment_stripper.h"

#include "diagnostics.h"

namespace pp {

namespace {
enum class State { Normal, LineComment, BlockComment, InString, InChar };
} // namespace

std::string stripComments(const std::string &source, const std::string &displayFilename) {
    std::string out;
    out.reserve(source.size());

    State state = State::Normal;
    int line = 1;
    int commentStartLine = 0;
    int literalStartLine = 0;

    const std::size_t n = source.size();
    std::size_t i = 0;

    while (i < n) {
        char c = source[i];
        char next = (i + 1 < n) ? source[i + 1] : '\0';

        switch (state) {
        case State::Normal:
            if (c == '/' && next == '/') {
                state = State::LineComment;
                i += 2;
            } else if (c == '/' && next == '*') {
                state = State::BlockComment;
                commentStartLine = line;
                i += 2;
            } else if (c == '"') {
                state = State::InString;
                literalStartLine = line;
                out += c;
                i += 1;
            } else if (c == '\'') {
                state = State::InChar;
                literalStartLine = line;
                out += c;
                i += 1;
            } else if (c == '\n') {
                out += c;
                line += 1;
                i += 1;
            } else {
                out += c;
                i += 1;
            }
            break;

        case State::LineComment:
            if (c == '\n') {
                state = State::Normal;
                out += c;
                line += 1;
                i += 1;
            } else {
                i += 1; // drop the character, comment absorbs it
            }
            break;

        case State::BlockComment:
            if (c == '*' && next == '/') {
                state = State::Normal;
                i += 2;
            } else if (c == '\n') {
                out += c; // preserve the newline so line numbers stay accurate
                line += 1;
                i += 1;
            } else {
                i += 1;
            }
            break;

        case State::InString:
        case State::InChar: {
            char quote = (state == State::InString) ? '"' : '\'';
            if (c == '\\' && i + 1 < n) {
                out += c;
                out += next;
                if (next == '\n') line += 1;
                i += 2;
            } else if (c == quote) {
                state = State::Normal;
                out += c;
                i += 1;
            } else if (c == '\n') {
                throw PreprocessorError(displayFilename, literalStartLine,
                                         "unterminated string/character literal");
            } else {
                out += c;
                i += 1;
            }
            break;
        }
        }
    }

    if (state == State::BlockComment) {
        throw PreprocessorError(displayFilename, commentStartLine, "unterminated block comment");
    }
    if (state == State::InString || state == State::InChar) {
        throw PreprocessorError(displayFilename, literalStartLine,
                                 "unterminated string/character literal");
    }
    // A // comment running off the end of the file needs no closing marker.

    return out;
}

} // namespace pp
