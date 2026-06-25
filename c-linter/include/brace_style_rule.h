#ifndef CL_BRACE_STYLE_RULE_H
#define CL_BRACE_STYLE_RULE_H

#include "diagnostic.h"
#include "token.h"

#include <string>
#include <vector>

namespace cl {

enum class BraceStyle { KandR, Allman };

// For each If/While token, finds the RightParen that closes its condition
// (tracking paren depth to handle nested parens) and, if a LeftBrace
// immediately follows, compares its line against the RightParen's line
// against the configured style. Occurrences with no LeftBrace immediately
// following the condition (braceless bodies, e.g. `if (x) foo();`) are out
// of scope and never flagged — this rule only compares brace placement
// when a brace is actually present.
std::vector<Diagnostic> checkBraceStyleRule(const std::vector<Token> &tokens,
                                             const std::string &filename, BraceStyle style);

} // namespace cl

#endif // CL_BRACE_STYLE_RULE_H
