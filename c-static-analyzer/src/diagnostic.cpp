#include "diagnostic.h"

#include <tuple>

namespace sa {

namespace {

auto key(const Diagnostic &d) {
    return std::tie(d.path, d.line, d.col, d.ruleId, d.message);
}

} // namespace

bool operator<(const Diagnostic &a, const Diagnostic &b) { return key(a) < key(b); }

bool operator==(const Diagnostic &a, const Diagnostic &b) { return key(a) == key(b); }

std::string toString(const Diagnostic &diagnostic) {
    return diagnostic.path + ":" + std::to_string(diagnostic.line) + ": " + diagnostic.ruleId + " " +
           diagnostic.message;
}

} // namespace sa
