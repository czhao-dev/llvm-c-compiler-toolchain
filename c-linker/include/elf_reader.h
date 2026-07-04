#ifndef CLNK_ELF_READER_H
#define CLNK_ELF_READER_H

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "object_file.h"

namespace clnk {

struct ReadResult {
    std::optional<ObjectFile> file;
    std::string error; // populated iff !file

    explicit operator bool() const { return file.has_value(); }
};

// Parses an in-memory ELF64 LSB x86-64 relocatable object (the same bytes
// `clang --target=x86_64-unknown-linux-gnu -c` produces). Never throws and
// never invokes undefined behavior on malformed input — every offset taken
// from the file is range-checked against `bytes.size()` before use.
// `sourceName` is attached to the result purely for diagnostics (e.g.
// "math.o"), it does not need to be a real path.
//
// Rejects (via ReadResult::error) anything outside this linker's scope:
// wrong magic/class/endianness/type/machine, more than one `.text` or
// `.data` section, a referenced `.bss` section, an `STB_WEAK` symbol, or a
// relocation type other than R_X86_64_64 / R_X86_64_PC32 / R_X86_64_PLT32.
// See docs/SPEC.md for the full list of accepted/dropped sections.
ReadResult readElfObject(const std::vector<std::byte> &bytes, const std::string &sourceName);

// Convenience: reads the file from disk first (I/O failure also reported
// via ReadResult::error, never an exception).
ReadResult readElfObject(const std::filesystem::path &path);

} // namespace clnk

#endif // CLNK_ELF_READER_H
