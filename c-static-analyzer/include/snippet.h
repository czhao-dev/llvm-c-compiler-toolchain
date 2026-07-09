#ifndef SA_SNIPPET_H
#define SA_SNIPPET_H

#include <cstddef>
#include <string>

#include "diagnostic.h"

namespace sa {

// Renders `sourceLine` (the exact text of the line the diagnostic points
// at, no trailing newline) followed by a caret line: `col` spaces then a
// single '^'. `col` is 0-indexed, matching Diagnostic::col, so the caret
// lines up directly under the offending character with no +/-1 needed.
std::string renderSnippet(const std::string &sourceLine, std::size_t col);

// toString(diagnostic) with a 1-indexed display column inserted
// ("path:line:col: ruleId message" -- the +1 is purely for human-readable
// display; renderSnippet() itself still uses the raw 0-indexed col for
// caret padding), followed by renderSnippet(). Opt-in counterpart to
// toString() -- used only when --show-source is passed.
std::string formatWithSource(const Diagnostic &diagnostic, const std::string &sourceLine);

} // namespace sa

#endif // SA_SNIPPET_H
