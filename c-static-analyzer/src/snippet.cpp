#include "snippet.h"

namespace sa {

std::string renderSnippet(const std::string &sourceLine, std::size_t col) {
    return sourceLine + "\n" + std::string(col, ' ') + "^";
}

std::string formatWithSource(const Diagnostic &diagnostic, const std::string &sourceLine) {
    std::string header = diagnostic.path + ":" + std::to_string(diagnostic.line) + ":" +
                          std::to_string(diagnostic.col + 1) + ": " + diagnostic.ruleId + " " + diagnostic.message;
    return header + "\n" + renderSnippet(sourceLine, diagnostic.col);
}

} // namespace sa
