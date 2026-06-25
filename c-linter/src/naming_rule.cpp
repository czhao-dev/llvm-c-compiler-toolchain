#include "naming_rule.h"

#include <algorithm>
#include <cctype>

namespace cl {

namespace {

bool isSnakeCaseViolation(const std::string &name) {
    bool hasUpper = std::any_of(name.begin(), name.end(), [](char c) {
        return std::isupper(static_cast<unsigned char>(c));
    });
    bool hasUnderscore = name.find('_') != std::string::npos;
    return hasUpper && !hasUnderscore;
}

} // namespace

std::vector<Diagnostic> checkNamingRule(const std::vector<Token> &tokens,
                                         const std::string &filename) {
    std::vector<Diagnostic> diagnostics;
    for (const Token &token : tokens) {
        if (token.type != TokenType::Identifier) continue;
        if (!isSnakeCaseViolation(token.lexeme)) continue;

        diagnostics.push_back(Diagnostic{Severity::Warning, filename, token.location.line,
                                          token.location.column, RuleCode::Naming,
                                          "identifier '" + token.lexeme +
                                              "' should be snake_case"});
    }
    return diagnostics;
}

} // namespace cl
