#include "relocation_applier.h"

#include <cstdint>
#include <cstring>
#include <limits>

namespace clnk {

namespace {

void writeLE(std::vector<std::byte> &bytes, std::uint64_t offset, unsigned width, std::uint64_t value) {
    for (unsigned i = 0; i < width; ++i) {
        bytes[offset + i] = static_cast<std::byte>((value >> (8 * i)) & 0xff);
    }
}

std::uint64_t fileSectionBase(std::uint32_t fileIndex, SectionKind kind, const MergedLayout &layout) {
    const auto &offsets = kind == SectionKind::Text ? layout.textFileOffset : layout.dataFileOffset;
    std::uint64_t base = kind == SectionKind::Text ? layout.textBase : layout.dataBase;
    return base + offsets[fileIndex].value_or(0);
}

} // namespace

std::uint64_t resolvedAddress(const ResolvedSymbol &symbol, const MergedLayout &layout) {
    return fileSectionBase(symbol.owningFileIndex, symbol.section, layout) + symbol.value;
}

void applyRelocations(const std::vector<ObjectFile> &files, const SymbolTable &table, MergedLayout &layout,
                       std::vector<Diagnostic> &diagnostics) {
    for (std::size_t fi = 0; fi < files.size(); ++fi) {
        const ObjectFile &file = files[fi];
        for (const Relocation &reloc : file.relocations) {
            const Symbol &sym = file.symbols[reloc.symbolIndex];

            std::optional<std::uint64_t> S;
            if (sym.location == SymbolLocation::Text || sym.location == SymbolLocation::Data) {
                SectionKind kind = sym.location == SymbolLocation::Text ? SectionKind::Text : SectionKind::Data;
                S = fileSectionBase(static_cast<std::uint32_t>(fi), kind, layout) + sym.value;
            } else if (sym.location == SymbolLocation::Undefined) {
                auto it = table.definitions.find(sym.name);
                if (it != table.definitions.end()) {
                    S = resolvedAddress(it->second, layout);
                }
                // If not found, checkUndefinedSymbols should already have
                // reported this; silently skip the patch rather than
                // double-report.
            } else {
                diagnostics.push_back(Diagnostic{Severity::Error, DiagnosticCode::UnsupportedInput, file.sourceName,
                                                  sym.name,
                                                  "relocation in " + file.sourceName + " targets symbol `" +
                                                      sym.name + "`, which is not part of .text or .data"});
            }

            if (!S) {
                continue;
            }

            std::uint64_t siteBase = fileSectionBase(static_cast<std::uint32_t>(fi), reloc.section, layout);
            std::uint64_t patchOffset = siteBase - (reloc.section == SectionKind::Text ? layout.textBase : layout.dataBase) + reloc.offset;
            std::vector<std::byte> &target = reloc.section == SectionKind::Text ? layout.text : layout.data;

            if (reloc.type == RelocationType::Abs64) {
                std::uint64_t value = static_cast<std::uint64_t>(static_cast<std::int64_t>(*S) + reloc.addend);
                writeLE(target, patchOffset, 8, value);
            } else {
                std::uint64_t P = siteBase + reloc.offset;
                std::int64_t result = static_cast<std::int64_t>(*S) + reloc.addend - static_cast<std::int64_t>(P);
                if (result < std::numeric_limits<std::int32_t>::min() || result > std::numeric_limits<std::int32_t>::max()) {
                    diagnostics.push_back(Diagnostic{
                        Severity::Error, DiagnosticCode::RelocationOverflow, file.sourceName, sym.name,
                        "relocation targeting `" + sym.name + "` in " + file.sourceName +
                            " does not fit in 32 bits after linking"});
                    continue;
                }
                writeLE(target, patchOffset, 4, static_cast<std::uint32_t>(static_cast<std::int32_t>(result)));
            }
        }
    }
}

} // namespace clnk
