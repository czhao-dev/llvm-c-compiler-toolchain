#include "diagnostic.h"

#include <algorithm>
#include <tuple>

namespace clnk {

namespace {
auto key(const Diagnostic &d) {
    return std::tie(d.file, d.symbol, d.code, d.message);
}
} // namespace

bool operator<(const Diagnostic &a, const Diagnostic &b) { return key(a) < key(b); }
bool operator==(const Diagnostic &a, const Diagnostic &b) { return key(a) == key(b); }

std::string toString(const Diagnostic &d) { return "error: " + d.message; }

void sortDiagnostics(std::vector<Diagnostic> &diagnostics) {
    std::sort(diagnostics.begin(), diagnostics.end());
}

} // namespace clnk
