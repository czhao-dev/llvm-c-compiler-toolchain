#ifndef SA_DIAGNOSTIC_H
#define SA_DIAGNOSTIC_H

#include <cstddef>
#include <string>

namespace sa {

// Field order is load-bearing: operator< compares fields top-to-bottom
// (path, line, col, ruleId, message), mirroring the original tool's sort
// key (itself modeled on a Python dataclass's order=True).
struct Diagnostic {
    std::string path;
    std::size_t line = 0;
    std::size_t col = 0;
    std::string ruleId;
    std::string message;
};

bool operator<(const Diagnostic &a, const Diagnostic &b);
bool operator==(const Diagnostic &a, const Diagnostic &b);

// Renders exactly "{path}:{line}: {ruleId} {message}" — no brackets, no
// column shown.
std::string toString(const Diagnostic &diagnostic);

} // namespace sa

#endif // SA_DIAGNOSTIC_H
