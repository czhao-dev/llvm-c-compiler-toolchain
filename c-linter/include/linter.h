#ifndef CL_LINTER_H
#define CL_LINTER_H

#include "brace_style_rule.h"
#include "diagnostic.h"

#include <string>
#include <vector>

namespace cl {

struct LinterOptions {
    int maxLineLength = 80;
    BraceStyle braceStyle = BraceStyle::KandR;
};

// The linter's single public entry point: pure (no file I/O — callers own
// reading files and reporting open failures), so it's directly testable on
// in-memory source strings.
class Linter {
public:
    explicit Linter(LinterOptions options = {});

    std::vector<Diagnostic> lintSource(const std::string &source,
                                        const std::string &filename) const;

private:
    LinterOptions options_;
};

} // namespace cl

#endif // CL_LINTER_H
