#ifndef CLNK_OBJECT_FILE_H
#define CLNK_OBJECT_FILE_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace clnk {

// Which merged output buffer a section's bytes belong to. Deliberately
// closed to these two — no .rodata/.bss/other section kinds (see
// docs/SPEC.md Non-Goals).
enum class SectionKind { Text, Data };

// Where a symbol's definition lives, from this linker's point of view.
// `Other` covers real ELF symbol kinds this linker never needs to resolve
// through (STT_FILE, STT_SECTION for a dropped section, symbols in
// SHN_ABS/SHN_COMMON) — they're kept in the table only so that
// `Relocation::symbolIndex` stays a valid index into the *original* ELF
// symbol table, but a relocation that actually references one is a
// diagnostic (`UnsupportedInput`), never resolved.
enum class SymbolLocation { Undefined, Text, Data, Other };

enum class SymbolBinding { Local, Global };

struct Symbol {
    std::string name;
    SymbolBinding binding = SymbolBinding::Local;
    SymbolLocation location = SymbolLocation::Undefined;
    std::uint64_t value = 0; // byte offset within the section named by `location`

    bool defined() const {
        return location == SymbolLocation::Text || location == SymbolLocation::Data;
    }
};

enum class RelocationType { Abs64, Pc32 };

struct Relocation {
    SectionKind section; // section being patched (.text or .data)
    std::uint32_t offset; // byte offset within that section's bytes
    std::uint32_t symbolIndex; // index into this file's `symbols`
    RelocationType type;
    std::int64_t addend;
};

// This linker's in-memory model of one input object file. Deliberately
// format-agnostic: nothing outside elf_reader.cpp ever touches an
// `elf::Elf64_*` struct directly, so the rest of the pipeline (merging,
// resolving, relocating) has no idea the input was ELF at all.
struct ObjectFile {
    std::string sourceName; // e.g. "math.o" — diagnostics only, never persisted

    std::vector<std::byte> text;
    std::uint32_t textAlign = 1;

    std::vector<std::byte> data;
    std::uint32_t dataAlign = 1;

    std::vector<Symbol> symbols;
    std::vector<Relocation> relocations;
};

} // namespace clnk

#endif // CLNK_OBJECT_FILE_H
