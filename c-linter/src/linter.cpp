#include "linter.h"

#include "lexer.h"
#include "line_rules.h"
#include "magic_number_rule.h"
#include "naming_rule.h"

#include <algorithm>

namespace cl {

Linter::Linter(LinterOptions options) : options_(options) {}

std::vector<Diagnostic> Linter::lintSource(const std::string &source,
                                            const std::string &filename) const {
    std::vector<Diagnostic> diagnostics = checkLineRules(source, filename, options_.maxLineLength);

    std::vector<Token> tokens = Lexer(source, filename).tokenize();

    std::vector<Diagnostic> naming = checkNamingRule(tokens, filename);
    std::vector<Diagnostic> magicNumbers = checkMagicNumberRule(tokens, filename);
    std::vector<Diagnostic> braceStyle = checkBraceStyleRule(tokens, filename, options_.braceStyle);

    diagnostics.insert(diagnostics.end(), naming.begin(), naming.end());
    diagnostics.insert(diagnostics.end(), magicNumbers.begin(), magicNumbers.end());
    diagnostics.insert(diagnostics.end(), braceStyle.begin(), braceStyle.end());

    std::sort(diagnostics.begin(), diagnostics.end());
    return diagnostics;
}

} // namespace cl
