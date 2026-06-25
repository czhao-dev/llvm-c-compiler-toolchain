#include "brace_style_rule.h"

namespace cl {

namespace {

bool isIfOrWhile(TokenType type) { return type == TokenType::If || type == TokenType::While; }

} // namespace

std::vector<Diagnostic> checkBraceStyleRule(const std::vector<Token> &tokens,
                                             const std::string &filename, BraceStyle style) {
    std::vector<Diagnostic> diagnostics;

    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (!isIfOrWhile(tokens[i].type)) continue;

        std::size_t openParen = i + 1;
        if (openParen >= tokens.size() || tokens[openParen].type != TokenType::LeftParen) {
            continue;
        }

        // Track paren depth to find the RightParen that truly closes this
        // condition (conditions may contain nested parens, e.g.
        // `if ((a + b) > c)`).
        int depth = 0;
        std::size_t closeParen = tokens.size(); // sentinel: not found
        for (std::size_t j = openParen; j < tokens.size(); ++j) {
            if (tokens[j].type == TokenType::LeftParen) {
                depth++;
            } else if (tokens[j].type == TokenType::RightParen) {
                depth--;
                if (depth == 0) {
                    closeParen = j;
                    break;
                }
            } else if (tokens[j].type == TokenType::EndOfFile) {
                break;
            }
        }
        if (closeParen == tokens.size()) continue; // truncated input, no closing paren

        std::size_t brace = closeParen + 1;
        if (brace >= tokens.size() || tokens[brace].type != TokenType::LeftBrace) {
            continue; // braceless body: out of scope for this rule
        }

        int parenLine = tokens[closeParen].location.line;
        int braceLine = tokens[brace].location.line;
        bool sameLine = (parenLine == braceLine);
        bool matchesStyle = (style == BraceStyle::KandR) ? sameLine : !sameLine;
        if (matchesStyle) continue;

        std::string styleName = (style == BraceStyle::KandR) ? "K&R" : "Allman";
        std::string keyword = (tokens[i].type == TokenType::If) ? "if" : "while";
        diagnostics.push_back(Diagnostic{
            Severity::Warning, filename, braceLine, tokens[brace].location.column,
            RuleCode::BraceStyle,
            "opening brace for '" + keyword + "' does not match configured " + styleName +
                " style"});
    }

    return diagnostics;
}

} // namespace cl
