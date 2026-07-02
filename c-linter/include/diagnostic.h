#ifndef CL_DIAGNOSTIC_H
#define CL_DIAGNOSTIC_H

#include <string>

namespace cl {

// A single severity today (all 5 rules are style/aesthetic, not
// correctness bugs), kept as an enum so a future rule could be promoted
// without changing the Diagnostic shape.
enum class Severity {
    Warning,
};

enum class RuleCode {
    Naming,             // CL001
    LineLength,         // CL002
    TrailingWhitespace, // CL003
    MagicNumber,        // CL004
    BraceStyle,         // CL005
};

std::string ruleCodeString(RuleCode code);
std::string severityName(Severity severity);

struct Diagnostic {
    Severity severity;
    std::string file;
    int line;
    int column;
    RuleCode ruleCode;
    std::string message;
};

// Orders by (line, column, ruleCode) — the sort key the Linter uses to
// present diagnostics for a single file in source order.
bool operator<(const Diagnostic &a, const Diagnostic &b);

// "file.c:12: warning: message [CL001]"
std::string format(const Diagnostic &diagnostic);

} // namespace cl

#endif // CL_DIAGNOSTIC_H
