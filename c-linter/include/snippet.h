#ifndef CL_SNIPPET_H
#define CL_SNIPPET_H

#include <string>

#include "diagnostic.h"

namespace cl {

// Renders `sourceLine` (the exact text of the line the diagnostic points
// at, no trailing newline) followed by a caret line: (column - 1) spaces
// then a single '^'. `column` is 1-indexed, matching Diagnostic::column.
std::string renderSnippet(const std::string &sourceLine, int column);

// format(diagnostic) with the column included in the header
// ("file:line:col: warning: message [CLxxx]"), followed by
// renderSnippet(sourceLine, diagnostic.column). Opt-in counterpart to
// format() -- used only when --show-source is passed.
std::string formatWithSource(const Diagnostic &diagnostic, const std::string &sourceLine);

} // namespace cl

#endif // CL_SNIPPET_H
