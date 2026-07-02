#include "line_rules.h"

#include <sstream>

namespace cl {

namespace {

// Strips a trailing '\r' so CRLF-terminated files don't inflate line
// length or get misreported as trailing whitespace.
std::string stripCarriageReturn(const std::string &line) {
    if (!line.empty() && line.back() == '\r') return line.substr(0, line.size() - 1);
    return line;
}

} // namespace

std::vector<Diagnostic> checkLineRules(const std::string &source, const std::string &filename,
                                        int maxLineLength) {
    std::vector<Diagnostic> diagnostics;
    std::istringstream stream(source);
    std::string rawLine;
    int lineNumber = 0;

    while (std::getline(stream, rawLine)) {
        lineNumber++;
        std::string line = stripCarriageReturn(rawLine);

        if (static_cast<int>(line.size()) > maxLineLength) {
            diagnostics.push_back(Diagnostic{
                Severity::Warning, filename, lineNumber, maxLineLength + 1, RuleCode::LineLength,
                "line exceeds " + std::to_string(maxLineLength) + " characters (" +
                    std::to_string(line.size()) + ")"});
        }

        if (!line.empty() && (line.back() == ' ' || line.back() == '\t')) {
            diagnostics.push_back(Diagnostic{Severity::Warning, filename, lineNumber,
                                              static_cast<int>(line.size()),
                                              RuleCode::TrailingWhitespace,
                                              "trailing whitespace"});
        }
    }

    return diagnostics;
}

} // namespace cl
