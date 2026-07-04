#ifndef CLNK_ELF64_H
#define CLNK_ELF64_H

#include <cstdint>

// Vendored subset of the ELF64 on-disk structures and constants this
// linker needs. macOS does not ship <elf.h>, so this header exists purely
// for portability across the platforms this project builds on (macOS,
// Linux) rather than relying on a system copy — the field names and
// values match the System V ABI / <elf.h> exactly, so anyone who does have
// a system elf.h will recognize every name here.
//
// Only the pieces this linker actually reads/writes are included: ELF64,
// little-endian, x86-64, relocatable (input) and executable (output)
// files. No 32-bit ELF, no other architectures, no dynamic-linking
// structures (no Elf64_Dyn, no .dynamic, no PT_INTERP/PT_DYNAMIC).

namespace clnk::elf {

using Elf64_Addr = std::uint64_t;
using Elf64_Off = std::uint64_t;
using Elf64_Half = std::uint16_t;
using Elf64_Word = std::uint32_t;
using Elf64_Sword = std::int32_t;
using Elf64_Xword = std::uint64_t;
using Elf64_Sxword = std::int64_t;

inline constexpr unsigned char kElfMag0 = 0x7f;
inline constexpr unsigned char kElfMag1 = 'E';
inline constexpr unsigned char kElfMag2 = 'L';
inline constexpr unsigned char kElfMag3 = 'F';

inline constexpr int EI_CLASS = 4;
inline constexpr int EI_DATA = 5;
inline constexpr int EI_VERSION = 6;
inline constexpr int EI_NIDENT = 16;

inline constexpr unsigned char ELFCLASS64 = 2;
inline constexpr unsigned char ELFDATA2LSB = 1;
inline constexpr unsigned char EV_CURRENT = 1;

inline constexpr Elf64_Half ET_REL = 1;
inline constexpr Elf64_Half ET_EXEC = 2;
inline constexpr Elf64_Half EM_X86_64 = 62;

struct Elf64_Ehdr {
    unsigned char e_ident[EI_NIDENT];
    Elf64_Half e_type;
    Elf64_Half e_machine;
    Elf64_Word e_version;
    Elf64_Addr e_entry;
    Elf64_Off e_phoff;
    Elf64_Off e_shoff;
    Elf64_Word e_flags;
    Elf64_Half e_ehsize;
    Elf64_Half e_phentsize;
    Elf64_Half e_phnum;
    Elf64_Half e_shentsize;
    Elf64_Half e_shnum;
    Elf64_Half e_shstrndx;
};

inline constexpr Elf64_Word SHT_NULL = 0;
inline constexpr Elf64_Word SHT_PROGBITS = 1;
inline constexpr Elf64_Word SHT_SYMTAB = 2;
inline constexpr Elf64_Word SHT_STRTAB = 3;
inline constexpr Elf64_Word SHT_RELA = 4;
inline constexpr Elf64_Word SHT_NOBITS = 8;

inline constexpr Elf64_Half SHN_UNDEF = 0;
inline constexpr Elf64_Half SHN_ABS = 0xfff1;
inline constexpr Elf64_Half SHN_COMMON = 0xfff2;
inline constexpr Elf64_Half SHN_LORESERVE = 0xff00;

struct Elf64_Shdr {
    Elf64_Word sh_name;
    Elf64_Word sh_type;
    Elf64_Xword sh_flags;
    Elf64_Addr sh_addr;
    Elf64_Off sh_offset;
    Elf64_Xword sh_size;
    Elf64_Word sh_link;
    Elf64_Word sh_info;
    Elf64_Xword sh_addralign;
    Elf64_Xword sh_entsize;
};

inline constexpr unsigned char STB_LOCAL = 0;
inline constexpr unsigned char STB_GLOBAL = 1;
inline constexpr unsigned char STB_WEAK = 2;

inline constexpr unsigned char STT_NOTYPE = 0;
inline constexpr unsigned char STT_OBJECT = 1;
inline constexpr unsigned char STT_FUNC = 2;
inline constexpr unsigned char STT_SECTION = 3;
inline constexpr unsigned char STT_FILE = 4;

struct Elf64_Sym {
    Elf64_Word st_name;
    unsigned char st_info;
    unsigned char st_other;
    Elf64_Half st_shndx;
    Elf64_Addr st_value;
    Elf64_Xword st_size;
};

inline constexpr unsigned char elf64StBind(unsigned char info) { return info >> 4; }
inline constexpr unsigned char elf64StType(unsigned char info) { return info & 0xf; }

struct Elf64_Rela {
    Elf64_Addr r_offset;
    Elf64_Xword r_info;
    Elf64_Sxword r_addend;
};

inline constexpr Elf64_Word elf64RSym(Elf64_Xword info) {
    return static_cast<Elf64_Word>(info >> 32);
}
inline constexpr Elf64_Word elf64RType(Elf64_Xword info) {
    return static_cast<Elf64_Word>(info & 0xffffffffu);
}

// Relocation types actually supported by this linker (see docs/SPEC.md for
// the full rationale). Any other type encountered in a .rela.text/.rela.data
// entry is a diagnostic, not a crash.
inline constexpr Elf64_Word R_X86_64_64 = 1;
inline constexpr Elf64_Word R_X86_64_PC32 = 2;
inline constexpr Elf64_Word R_X86_64_PLT32 = 4;

struct Elf64_Phdr {
    Elf64_Word p_type;
    Elf64_Word p_flags;
    Elf64_Off p_offset;
    Elf64_Addr p_vaddr;
    Elf64_Addr p_paddr;
    Elf64_Xword p_filesz;
    Elf64_Xword p_memsz;
    Elf64_Xword p_align;
};

inline constexpr Elf64_Word PT_LOAD = 1;
inline constexpr Elf64_Word PF_X = 1;
inline constexpr Elf64_Word PF_W = 2;
inline constexpr Elf64_Word PF_R = 4;

static_assert(sizeof(Elf64_Ehdr) == 64, "Elf64_Ehdr must match the on-disk ABI layout");
static_assert(sizeof(Elf64_Shdr) == 64, "Elf64_Shdr must match the on-disk ABI layout");
static_assert(sizeof(Elf64_Sym) == 24, "Elf64_Sym must match the on-disk ABI layout");
static_assert(sizeof(Elf64_Rela) == 24, "Elf64_Rela must match the on-disk ABI layout");
static_assert(sizeof(Elf64_Phdr) == 56, "Elf64_Phdr must match the on-disk ABI layout");

} // namespace clnk::elf

#endif // CLNK_ELF64_H
