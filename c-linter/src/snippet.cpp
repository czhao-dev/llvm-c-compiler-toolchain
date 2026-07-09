#include "snippet.h"

namespace cl {

std::string renderSnippet(const std::string &sourceLine, int column) {
    std::string caretLine(column > 1 ? static_cast<std::size_t>(column - 1) : 0, ' ');
    caretLine += '^';
    return sourceLine + "\n" + caretLine;
}

std::string formatWithSource(const Diagnostic &diagnostic, const std::string &sourceLine) {
    std::string header = diagnostic.file + ":" + std::to_string(diagnostic.line) + ":" +
                          std::to_string(diagnostic.column) + ": " + severityName(diagnostic.severity) + ": " +
                          diagnostic.message + " [" + ruleCodeString(diagnostic.ruleCode) + "]";
    return header + "\n" + renderSnippet(sourceLine, diagnostic.column);
}

} // namespace cl
