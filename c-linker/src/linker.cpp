#include "linker.h"

#include "elf_reader.h"
#include "relocation_applier.h"
#include "section_merger.h"
#include "symbol_resolver.h"

namespace clnk {

namespace {
std::uint64_t alignUp(std::uint64_t value, std::uint64_t align) {
    return ((value + align - 1) / align) * align;
}
} // namespace

LinkResult linkObjects(std::vector<ObjectFile> files, const LinkOptions &options) {
    LinkResult result;

    SymbolTable table = buildSymbolTable(files, result.diagnostics);
    bool undefinedOk = checkUndefinedSymbols(files, table, result.diagnostics);

    if (!result.diagnostics.empty() || !undefinedOk) {
        sortDiagnostics(result.diagnostics);
        result.ok = false;
        return result;
    }

    MergedLayout layout = mergeSections(files, options.textBase, options.dataBase);
    if (options.dataBase == 0) {
        layout.dataBase = alignUp(layout.textBase + layout.text.size(), kPageSize);
    }

    auto entryIt = table.definitions.find(options.entrySymbol);
    if (entryIt == table.definitions.end()) {
        result.diagnostics.push_back(Diagnostic{Severity::Error, DiagnosticCode::UndefinedSymbol, "",
                                                  options.entrySymbol,
                                                  "undefined entry symbol `" + options.entrySymbol + "`"});
        result.ok = false;
        return result;
    }

    applyRelocations(files, table, layout, result.diagnostics);
    if (!result.diagnostics.empty()) {
        sortDiagnostics(result.diagnostics);
        result.ok = false;
        return result;
    }

    result.image.entryPoint = resolvedAddress(entryIt->second, layout);
    result.image.textBase = layout.textBase;
    result.image.text = std::move(layout.text);
    result.image.dataBase = layout.dataBase;
    result.image.data = std::move(layout.data);
    result.ok = true;
    return result;
}

LinkResult link(const LinkOptions &options) {
    std::vector<ObjectFile> files;
    files.reserve(options.inputPaths.size());
    for (const auto &path : options.inputPaths) {
        ReadResult read = readElfObject(path);
        if (!read) {
            LinkResult result;
            result.ok = false;
            result.diagnostics.push_back(
                Diagnostic{Severity::Error, DiagnosticCode::MalformedObjectFile, path.string(), "", read.error});
            return result;
        }
        files.push_back(std::move(*read.file));
    }
    return linkObjects(std::move(files), options);
}

} // namespace clnk
