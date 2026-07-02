#ifndef CL_LINE_RULES_H
#define CL_LINE_RULES_H

#include "diagnostic.h"

#include <string>
#include <vector>

namespace cl {

// Pure raw-text pass over `source`, run before tokenization. Flags lines
// longer than maxLineLength (CL002) and lines with trailing whitespace
// (CL003) — independently, so a single line can emit both.
std::vector<Diagnostic> checkLineRules(const std::string &source, const std::string &filename,
                                        int maxLineLength);

} // namespace cl

#endif // CL_LINE_RULES_H
