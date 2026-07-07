#ifndef CLNK_DIAGNOSTIC_H
#define CLNK_DIAGNOSTIC_H

#include <string>
#include <vector>

namespace clnk {

// Static linking failures are always fatal to the overall link — there is
// no "warning" tier today (see docs/SPEC.md).
enum class Severity { Error };

enum class DiagnosticCode {
    MalformedObjectFile, // not a well-formed ELF64 x86-64 relocatable file
    UnsupportedInput,    // a real ELF feature outside this linker's scope (weak
                          // binding, .bss, multiple .text sections, a relocation
                          // that targets a dropped/unsupported symbol, ...)
    UnsupportedRelocationType,
    UndefinedSymbol,
    MultipleDefinition,
    RelocationOverflow,
};

struct Diagnostic {
    Severity severity = Severity::Error;
    DiagnosticCode code;
    std::string file;    // the input object involved, if any
    std::string symbol;  // the symbol name involved, if any
    std::string message; // fully rendered human-readable text
};

bool operator<(const Diagnostic &a, const Diagnostic &b);
bool operator==(const Diagnostic &a, const Diagnostic &b);

// "error: {message}" — matches the rendering convention of sibling
// subprojects' Diagnostic::toString.
std::string toString(const Diagnostic &d);

void sortDiagnostics(std::vector<Diagnostic> &diagnostics);

} // namespace clnk

#endif // CLNK_DIAGNOSTIC_H
