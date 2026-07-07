#ifndef CLNK_RELOCATION_APPLIER_H
#define CLNK_RELOCATION_APPLIER_H

#include <cstdint>
#include <vector>

#include "diagnostic.h"
#include "object_file.h"
#include "section_merger.h"
#include "symbol_resolver.h"

namespace clnk {

// Absolute load address of a resolved definition, given the merged layout.
std::uint64_t resolvedAddress(const ResolvedSymbol &symbol, const MergedLayout &layout);

// Walks every relocation in every input file and patches the corresponding
// bytes in place inside layout.text/layout.data.
//
// Patch semantics (see docs/SPEC.md): let S be the resolved symbol's final
// absolute address, A the relocation's addend, and P the relocation site's
// own final absolute address (i.e. r_offset's address after merging, NOT
// offset-plus-width -- this matches the real ELF x86-64 psABI, where the
// compiler already bakes any "distance to next instruction" adjustment
// into A itself).
//   Abs64: write64(S + A)
//   Pc32:  write32(S + A - P)   -- also used for PLT32, since no PLT exists
//
// Callers must have already run buildSymbolTable + checkUndefinedSymbols
// successfully; a relocation whose symbol is still unresolvable (e.g. it
// targets a symbol from an unsupported/dropped section) produces an
// UnsupportedInput diagnostic and leaves those bytes unpatched, and a Pc32
// result that doesn't fit int32_t produces RelocationOverflow.
void applyRelocations(const std::vector<ObjectFile> &files, const SymbolTable &table, MergedLayout &layout,
                       std::vector<Diagnostic> &diagnostics);

} // namespace clnk

#endif // CLNK_RELOCATION_APPLIER_H
