#include "magic_number_rule.h"

#include <cstdlib>

namespace cl {

namespace {

bool isComparisonOperator(TokenType type) {
    switch (type) {
    case TokenType::EqualEqual:
    case TokenType::NotEqual:
    case TokenType::Less:
    case TokenType::Greater:
    case TokenType::LessEqual:
    case TokenType::GreaterEqual:
        return true;
    default:
        return false;
    }
}

// Strips trailing integer suffix letters (u/U/l/L in any combination) so
// strtoll can parse the value; base 0 also handles 0x.. hex and 0.. octal
// prefixes the same way C itself does.
long long parseIntLiteralValue(const std::string &lexeme) {
    std::string digits = lexeme;
    while (!digits.empty()) {
        char c = digits.back();
        if (c == 'u' || c == 'U' || c == 'l' || c == 'L') {
            digits.pop_back();
        } else {
            break;
        }
    }
    if (digits.empty()) return 0;
    return std::strtoll(digits.c_str(), nullptr, 0);
}

bool isExemptValue(long long value) { return value == 0 || value == 1 || value == -1; }

} // namespace

std::vector<Diagnostic> checkMagicNumberRule(const std::vector<Token> &tokens,
                                              const std::string &filename) {
    std::vector<Diagnostic> diagnostics;

    for (std::size_t i = 0; i + 1 < tokens.size(); ++i) {
        if (!isComparisonOperator(tokens[i].type)) continue;

        std::size_t literalIndex = i + 1;
        bool negative = false;
        if (tokens[literalIndex].type == TokenType::Other && tokens[literalIndex].lexeme == "-" &&
            literalIndex + 1 < tokens.size()) {
            negative = true;
            literalIndex++;
        }

        if (tokens[literalIndex].type != TokenType::IntLiteral) continue;

        long long value = parseIntLiteralValue(tokens[literalIndex].lexeme);
        if (negative) value = -value;
        if (isExemptValue(value)) continue;

        const Token &locationToken = negative ? tokens[i + 1] : tokens[literalIndex];
        std::string literalText = (negative ? "-" : "") + tokens[literalIndex].lexeme;

        diagnostics.push_back(Diagnostic{
            Severity::Warning, filename, locationToken.location.line, locationToken.location.column,
            RuleCode::MagicNumber,
            "magic number '" + literalText + "' in comparison; consider a named macro"});
    }

    return diagnostics;
}

} // namespace cl
