#include "pp_tokenizer.h"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "diagnostics.h"

namespace {

std::vector<pp::PPToken> tokenize(const std::string &text) {
    return pp::PPTokenizer(text).tokenize();
}

void expectRoundTrip(const std::string &text) {
    std::string rebuilt;
    for (const auto &tok : tokenize(text)) rebuilt += tok.text;
    if (rebuilt != text) {
        std::cerr << "round-trip mismatch: \"" << text << "\" -> \"" << rebuilt << "\"\n";
        std::abort();
    }
}

void expectSingleToken(const std::string &text, pp::PPTokenKind kind) {
    auto tokens = tokenize(text);
    assert(tokens.size() == 1);
    assert(tokens[0].kind == kind);
    assert(tokens[0].text == text);
}

} // namespace

int main() {
    using pp::PPTokenKind;

    expectRoundTrip("int x = a->b + 0X10 * 3.14f;");
    expectRoundTrip("const char *s = \"a \\\"quoted\\\" word\";");
    expectRoundTrip("   \t  ");
    expectRoundTrip("");

    // Maximal munch: XY is one identifier, never X + Y.
    expectSingleToken("XY", PPTokenKind::Identifier);

    // pp-number handling: a leading digit absorbs following letters, so a
    // macro literally named X could never match inside 0X10.
    expectSingleToken("0X10", PPTokenKind::Number);
    expectSingleToken("3.14f", PPTokenKind::Number);

    // String/char literals are consumed atomically, including escapes.
    expectSingleToken("\"she said \\\"hi\\\"\"", PPTokenKind::StringLiteral);
    expectSingleToken("'a'", PPTokenKind::CharLiteral);
    expectSingleToken("'\\n'", PPTokenKind::CharLiteral);
    expectSingleToken("'\\''", PPTokenKind::CharLiteral);

    // Whitespace runs collapse into a single token.
    {
        auto tokens = tokenize("a    b");
        assert(tokens.size() == 3);
        assert(tokens[0].kind == PPTokenKind::Identifier && tokens[0].text == "a");
        assert(tokens[1].kind == PPTokenKind::Whitespace && tokens[1].text == "    ");
        assert(tokens[2].kind == PPTokenKind::Identifier && tokens[2].text == "b");
    }

    // Multi-character operators are deliberately NOT recognized as units.
    {
        auto tokens = tokenize("a->b");
        assert(tokens.size() == 4);
        assert(tokens[0].kind == PPTokenKind::Identifier && tokens[0].text == "a");
        assert(tokens[1].kind == PPTokenKind::Punct && tokens[1].text == "-");
        assert(tokens[2].kind == PPTokenKind::Punct && tokens[2].text == ">");
        assert(tokens[3].kind == PPTokenKind::Identifier && tokens[3].text == "b");
    }

    // Unterminated literal at end of text is an error (defensive case).
    try {
        tokenize("\"never closed");
        std::cerr << "expected unterminated literal to throw\n";
        std::abort();
    } catch (const pp::PreprocessorError &) {
        // expected
    }

    // Empty input yields no tokens.
    assert(tokenize("").empty());

    std::cout << "pp_tokenizer_test: all checks passed\n";
    return 0;
}
