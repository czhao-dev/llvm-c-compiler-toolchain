#include "diagnostic.h"

namespace cl {

std::string ruleCodeString(RuleCode code) {
    switch (code) {
    case RuleCode::Naming: return "CL001";
    case RuleCode::LineLength: return "CL002";
    case RuleCode::TrailingWhitespace: return "CL003";
    case RuleCode::MagicNumber: return "CL004";
    case RuleCode::BraceStyle: return "CL005";
    }
    return "CL000";
}

std::string severityName(Severity severity) {
    switch (severity) {
    case Severity::Warning: return "warning";
    }
    return "unknown";
}

bool operator<(const Diagnostic &a, const Diagnostic &b) {
    if (a.line != b.line) return a.line < b.line;
    if (a.column != b.column) return a.column < b.column;
    return ruleCodeString(a.ruleCode) < ruleCodeString(b.ruleCode);
}

std::string format(const Diagnostic &diagnostic) {
    return diagnostic.file + ":" + std::to_string(diagnostic.line) + ": " +
           severityName(diagnostic.severity) + ": " + diagnostic.message + " [" +
           ruleCodeString(diagnostic.ruleCode) + "]";
}

} // namespace cl
