#ifndef CLNK_LINKER_H
#define CLNK_LINKER_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "diagnostic.h"
#include "object_file.h"

namespace clnk {

// Every PT_LOAD segment in the emitted executable is page-aligned (a real
// ELF/Linux loader requirement: p_vaddr must be congruent to p_offset mod
// p_align). One whole page is reserved below `textBase` for the ELF
// header + program headers, so `textBase` itself is exactly where the
// merged .text bytes start -- there is no header-sized offset to account
// for anywhere else in the pipeline.
inline constexpr std::uint64_t kPageSize = 0x1000;

struct LinkOptions {
    std::vector<std::filesystem::path> inputPaths;
    std::filesystem::path outputPath;
    std::string entrySymbol = "_start";
    std::uint64_t textBase = 0x401000; // must be a multiple of kPageSize, >= kPageSize
    std::uint64_t dataBase = 0;        // 0 = auto: next page boundary after .text; else must be a multiple of kPageSize
};

struct LinkedImage {
    std::uint64_t entryPoint = 0;
    std::uint64_t textBase = 0;
    std::vector<std::byte> text;
    std::uint64_t dataBase = 0;
    std::vector<std::byte> data;
};

struct LinkResult {
    bool ok = false;
    std::vector<Diagnostic> diagnostics;
    LinkedImage image; // meaningful only when ok
};

// The disk-free core pipeline: build the symbol table, check for undefined
// symbols, merge sections, resolve the entry point, apply relocations, and
// assemble a LinkedImage. Reusable directly by tests without touching the
// filesystem.
LinkResult linkObjects(std::vector<ObjectFile> files, const LinkOptions &options);

// Reads every input file from disk (via readElfObject) and delegates to
// linkObjects(). A file that fails to parse produces a MalformedObjectFile
// diagnostic and aborts the link before any resolution/merging happens.
LinkResult link(const LinkOptions &options);

} // namespace clnk

#endif // CLNK_LINKER_H
