#ifndef CLNK_SECTION_MERGER_H
#define CLNK_SECTION_MERGER_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "object_file.h"

namespace clnk {

struct MergedLayout {
    std::uint64_t textBase = 0;
    std::uint64_t dataBase = 0;
    std::vector<std::byte> text;
    std::vector<std::byte> data;

    // Indexed by input file index. nullopt if that file contributed no
    // section of this kind (e.g. a file with no .data). Otherwise, the
    // byte offset within `text`/`data` where that file's bytes start --
    // add textBase/dataBase to get the absolute load address.
    std::vector<std::optional<std::uint64_t>> textFileOffset;
    std::vector<std::optional<std::uint64_t>> dataFileOffset;
};

// Concatenates every input file's .text (respectively .data), in input
// order, zero-padding between files so each file's section starts at an
// offset satisfying its own alignment.
MergedLayout mergeSections(const std::vector<ObjectFile> &files, std::uint64_t textBase, std::uint64_t dataBase);

} // namespace clnk

#endif // CLNK_SECTION_MERGER_H
