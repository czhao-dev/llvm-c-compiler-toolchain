#ifndef CLNK_ELF_WRITER_H
#define CLNK_ELF_WRITER_H

#include <cstddef>
#include <vector>

#include "linker.h"

namespace clnk {

// Emits a minimal, real, loadable static ET_EXEC ELF64 x86-64 executable,
// runnable on x86-64 Linux. Layout: one kPageSize page holding the ELF
// header and two PT_LOAD program headers (zero-padded to the page
// boundary), then the merged .text bytes (loaded at image.textBase),
// padding to the next page boundary, then the merged .data bytes (loaded
// at image.dataBase). No section headers are emitted -- a stripped static
// executable doesn't need them to run.
//
// Requires image.textBase and image.dataBase to be multiples of
// kPageSize, and image.textBase >= kPageSize (room for the header page
// below it, per the ELF rule that p_vaddr must be congruent to p_offset
// modulo p_align for every PT_LOAD segment).
std::vector<std::byte> writeElfExecutable(const LinkedImage &image);

} // namespace clnk

#endif // CLNK_ELF_WRITER_H
