#include "symbol_resolver.h"

#include <algorithm>
#include <map>
#include <set>

namespace clnk {

SymbolTable buildSymbolTable(const std::vector<ObjectFile> &files, std::vector<Diagnostic> &diagnostics) {
    SymbolTable table;
    for (std::size_t fi = 0; fi < files.size(); ++fi) {
        const ObjectFile &file = files[fi];
        for (const Symbol &sym : file.symbols) {
            if (sym.binding != SymbolBinding::Global || !sym.defined()) {
                continue;
            }
            auto [it, inserted] = table.definitions.try_emplace(
                sym.name,
                ResolvedSymbol{sym.name, static_cast<std::uint32_t>(fi), sym.location == SymbolLocation::Text ? SectionKind::Text : SectionKind::Data,
                               sym.value});
            if (!inserted) {
                const std::string &firstFile = files[it->second.owningFileIndex].sourceName;
                diagnostics.push_back(Diagnostic{
                    Severity::Error, DiagnosticCode::MultipleDefinition, file.sourceName, sym.name,
                    "multiple definition of `" + sym.name + "`: first defined in " + firstFile +
                        ", also defined in " + file.sourceName});
            }
        }
    }
    return table;
}

bool checkUndefinedSymbols(const std::vector<ObjectFile> &files, const SymbolTable &table,
                            std::vector<Diagnostic> &diagnostics) {
    std::map<std::string, std::set<std::string>> missingReferences;

    for (const ObjectFile &file : files) {
        for (const Relocation &reloc : file.relocations) {
            const Symbol &sym = file.symbols[reloc.symbolIndex];
            if (sym.location != SymbolLocation::Undefined) {
                continue;
            }
            if (table.definitions.find(sym.name) == table.definitions.end()) {
                missingReferences[sym.name].insert(file.sourceName);
            }
        }
    }

    for (const auto &[name, referencingFiles] : missingReferences) {
        std::string files_list;
        for (const std::string &f : referencingFiles) {
            if (!files_list.empty()) files_list += ", ";
            files_list += f;
        }
        diagnostics.push_back(Diagnostic{Severity::Error, DiagnosticCode::UndefinedSymbol, referencingFiles.empty() ? "" : *referencingFiles.begin(),
                                          name,
                                          "undefined symbol `" + name + "` referenced in " + files_list});
    }

    return missingReferences.empty();
}

} // namespace clnk
