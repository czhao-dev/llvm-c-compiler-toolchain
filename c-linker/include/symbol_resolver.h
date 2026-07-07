#ifndef CLNK_SYMBOL_RESOLVER_H
#define CLNK_SYMBOL_RESOLVER_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "diagnostic.h"
#include "object_file.h"

namespace clnk {

struct ResolvedSymbol {
    std::string name;
    std::uint32_t owningFileIndex = 0; // index into the input file list
    SectionKind section = SectionKind::Text;
    std::uint64_t value = 0; // byte offset within that file's own section
};

struct SymbolTable {
    // Global defined symbols only, keyed by name. Local symbols never enter
    // this table -- they never conflict across files and never satisfy
    // another file's undefined reference.
    std::unordered_map<std::string, ResolvedSymbol> definitions;
};

// Records every Global defined symbol from every file. A name defined
// Global more than once (across files, or twice within one file) produces
// a MultipleDefinition diagnostic naming every file involved; the first
// definition encountered is kept in `definitions` so the rest of the
// pipeline has something to resolve against, but the link still fails
// overall whenever any diagnostic was produced.
SymbolTable buildSymbolTable(const std::vector<ObjectFile> &files, std::vector<Diagnostic> &diagnostics);

// For every relocation in every file that targets an undefined symbol,
// verifies `table` has a matching definition. Emits one UndefinedSymbol
// diagnostic per offending name (sorted for determinism). Returns true iff
// no undefined symbols were found.
bool checkUndefinedSymbols(const std::vector<ObjectFile> &files, const SymbolTable &table,
                            std::vector<Diagnostic> &diagnostics);

} // namespace clnk

#endif // CLNK_SYMBOL_RESOLVER_H
