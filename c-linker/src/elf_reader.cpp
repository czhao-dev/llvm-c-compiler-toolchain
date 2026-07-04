#include "elf_reader.h"

#include <cstring>
#include <fstream>
#include <optional>

#include "elf64.h"

namespace clnk {

using namespace elf;

namespace {

bool inBounds(std::uint64_t total, std::uint64_t offset, std::uint64_t size) {
    return offset <= total && size <= total - offset;
}

template <typename T>
std::optional<T> readStruct(const std::vector<std::byte> &bytes, std::uint64_t offset) {
    if (!inBounds(bytes.size(), offset, sizeof(T))) {
        return std::nullopt;
    }
    T value;
    std::memcpy(&value, bytes.data() + offset, sizeof(T));
    return value;
}

// Reads a NUL-terminated string starting at `tableOffset + index`, bounded
// by `[tableOffset, tableOffset + tableSize)`.
std::optional<std::string> readCString(const std::vector<std::byte> &bytes, std::uint64_t tableOffset,
                                        std::uint64_t tableSize, std::uint32_t index) {
    if (index >= tableSize) {
        return std::nullopt;
    }
    const char *base = reinterpret_cast<const char *>(bytes.data() + tableOffset);
    std::uint64_t max = tableSize - index;
    const void *nul = std::memchr(base + index, '\0', max);
    if (nul == nullptr) {
        return std::nullopt;
    }
    return std::string(base + index);
}

// Carries a failure message out of a nested helper so callers can bail out
// of readElfObject with a single `return fail(err->message)`.
struct Fail {
    std::string message;
};

struct RawSection {
    std::string name;
    Elf64_Shdr header;
};

} // namespace

ReadResult readElfObject(const std::vector<std::byte> &bytes, const std::string &sourceName) {
    auto fail = [&](std::string message) -> ReadResult {
        return ReadResult{std::nullopt, sourceName + ": " + std::move(message)};
    };

    auto ehdrOpt = readStruct<Elf64_Ehdr>(bytes, 0);
    if (!ehdrOpt) {
        return fail("truncated file: missing ELF header");
    }
    const Elf64_Ehdr &ehdr = *ehdrOpt;

    if (ehdr.e_ident[0] != kElfMag0 || ehdr.e_ident[1] != kElfMag1 || ehdr.e_ident[2] != kElfMag2 ||
        ehdr.e_ident[3] != kElfMag3) {
        return fail("not an ELF file (bad magic)");
    }
    if (ehdr.e_ident[EI_CLASS] != ELFCLASS64) {
        return fail("not a 64-bit ELF file (only ELF64 is supported)");
    }
    if (ehdr.e_ident[EI_DATA] != ELFDATA2LSB) {
        return fail("not a little-endian ELF file (only LSB is supported)");
    }
    if (ehdr.e_ident[EI_VERSION] != EV_CURRENT) {
        return fail("unsupported ELF identification version");
    }
    if (ehdr.e_type != ET_REL) {
        return fail("not a relocatable object file (ET_REL) -- archives and executables are unsupported input");
    }
    if (ehdr.e_machine != EM_X86_64) {
        return fail("not an x86-64 object file (only EM_X86_64 is supported)");
    }
    if (ehdr.e_shentsize != sizeof(Elf64_Shdr)) {
        return fail("unexpected section header entry size");
    }
    if (ehdr.e_shnum == 0) {
        return fail("object file has no sections");
    }
    if (ehdr.e_shstrndx >= ehdr.e_shnum) {
        return fail("section header string table index out of range");
    }

    std::vector<Elf64_Shdr> sectionHeaders;
    sectionHeaders.reserve(ehdr.e_shnum);
    for (Elf64_Half i = 0; i < ehdr.e_shnum; ++i) {
        auto shdr = readStruct<Elf64_Shdr>(bytes, ehdr.e_shoff + static_cast<std::uint64_t>(i) * ehdr.e_shentsize);
        if (!shdr) {
            return fail("truncated file: missing section header");
        }
        sectionHeaders.push_back(*shdr);
    }

    const Elf64_Shdr &shstrtab = sectionHeaders[ehdr.e_shstrndx];
    if (shstrtab.sh_type != SHT_STRTAB || !inBounds(bytes.size(), shstrtab.sh_offset, shstrtab.sh_size)) {
        return fail("malformed section header string table");
    }

    std::vector<RawSection> sections(sectionHeaders.size());
    for (std::size_t i = 0; i < sectionHeaders.size(); ++i) {
        auto name = readCString(bytes, shstrtab.sh_offset, shstrtab.sh_size, sectionHeaders[i].sh_name);
        if (!name) {
            return fail("malformed section name");
        }
        sections[i] = RawSection{*name, sectionHeaders[i]};
    }

    // Locate the sections this linker cares about by name. Everything else
    // (.rodata, .bss, .comment, .note.*, .eh_frame, .debug_*, ...) is
    // simply never looked up and therefore silently dropped.
    std::optional<std::uint16_t> textIdx, dataIdx, symtabIdx, relaTextIdx, relaDataIdx;
    for (std::size_t i = 0; i < sections.size(); ++i) {
        const std::string &name = sections[i].name;
        const Elf64_Shdr &shdr = sections[i].header;
        auto claim = [&](std::optional<std::uint16_t> &slot, const char *what) -> std::optional<Fail> {
            if (slot.has_value()) {
                return Fail{std::string("multiple ") + what + " sections are unsupported"};
            }
            slot = static_cast<std::uint16_t>(i);
            return std::nullopt;
        };
        if (name == ".text" && shdr.sh_type == SHT_PROGBITS) {
            if (auto err = claim(textIdx, ".text")) return fail(err->message);
        } else if (name == ".data" && shdr.sh_type == SHT_PROGBITS) {
            if (auto err = claim(dataIdx, ".data")) return fail(err->message);
        } else if (name == ".symtab" && shdr.sh_type == SHT_SYMTAB) {
            if (auto err = claim(symtabIdx, ".symtab")) return fail(err->message);
        } else if (name == ".rela.text" && shdr.sh_type == SHT_RELA) {
            if (auto err = claim(relaTextIdx, ".rela.text")) return fail(err->message);
        } else if (name == ".rela.data" && shdr.sh_type == SHT_RELA) {
            if (auto err = claim(relaDataIdx, ".rela.data")) return fail(err->message);
        }
    }

    ObjectFile object;
    object.sourceName = sourceName;

    auto loadBytes = [&](std::uint16_t idx) -> std::optional<std::vector<std::byte>> {
        const Elf64_Shdr &shdr = sections[idx].header;
        if (shdr.sh_type == SHT_NOBITS) {
            return std::nullopt; // shouldn't happen for .text/.data by name+type match above
        }
        if (!inBounds(bytes.size(), shdr.sh_offset, shdr.sh_size)) {
            return std::nullopt;
        }
        return std::vector<std::byte>(bytes.begin() + shdr.sh_offset, bytes.begin() + shdr.sh_offset + shdr.sh_size);
    };

    if (textIdx) {
        auto raw = loadBytes(*textIdx);
        if (!raw) return fail(".text section data is out of range");
        object.text = std::move(*raw);
        object.textAlign = static_cast<std::uint32_t>(sections[*textIdx].header.sh_addralign ? sections[*textIdx].header.sh_addralign : 1);
    }
    if (dataIdx) {
        auto raw = loadBytes(*dataIdx);
        if (!raw) return fail(".data section data is out of range");
        object.data = std::move(*raw);
        object.dataAlign = static_cast<std::uint32_t>(sections[*dataIdx].header.sh_addralign ? sections[*dataIdx].header.sh_addralign : 1);
    }

    if ((relaTextIdx || relaDataIdx) && !symtabIdx) {
        return fail("relocations present but no .symtab found");
    }

    // Symbol table: preserve the original .symtab ordering exactly, since
    // relocation entries reference symbols by that index.
    std::string strtabView;
    std::uint64_t strtabOffset = 0, strtabSize = 0;
    if (symtabIdx) {
        const Elf64_Shdr &symtabHdr = sections[*symtabIdx].header;
        if (symtabHdr.sh_entsize != sizeof(Elf64_Sym)) {
            return fail("unexpected symbol table entry size");
        }
        if (symtabHdr.sh_link >= sections.size()) {
            return fail("symbol table's linked string table index is out of range");
        }
        const Elf64_Shdr &strtabHdr = sections[symtabHdr.sh_link].header;
        if (strtabHdr.sh_type != SHT_STRTAB || !inBounds(bytes.size(), strtabHdr.sh_offset, strtabHdr.sh_size)) {
            return fail("malformed symbol string table");
        }
        strtabOffset = strtabHdr.sh_offset;
        strtabSize = strtabHdr.sh_size;

        if (symtabHdr.sh_size % symtabHdr.sh_entsize != 0) {
            return fail("symbol table size is not a multiple of its entry size");
        }
        std::uint64_t count = symtabHdr.sh_size / symtabHdr.sh_entsize;
        object.symbols.reserve(count);
        for (std::uint64_t i = 0; i < count; ++i) {
            auto sym = readStruct<Elf64_Sym>(bytes, symtabHdr.sh_offset + i * symtabHdr.sh_entsize);
            if (!sym) return fail("truncated symbol table");

            auto name = readCString(bytes, strtabOffset, strtabSize, sym->st_name);
            if (!name) return fail("malformed symbol name");

            unsigned char bind = elf64StBind(sym->st_info);
            SymbolBinding binding;
            if (bind == STB_LOCAL) {
                binding = SymbolBinding::Local;
            } else if (bind == STB_GLOBAL) {
                binding = SymbolBinding::Global;
            } else {
                return fail("symbol '" + *name + "' has an unsupported binding (only LOCAL/GLOBAL are supported)");
            }

            SymbolLocation location;
            if (sym->st_shndx == SHN_UNDEF) {
                location = SymbolLocation::Undefined;
            } else if (textIdx && sym->st_shndx == *textIdx) {
                location = SymbolLocation::Text;
            } else if (dataIdx && sym->st_shndx == *dataIdx) {
                location = SymbolLocation::Data;
            } else {
                location = SymbolLocation::Other;
            }

            object.symbols.push_back(Symbol{*name, binding, location, sym->st_value});
        }
    }

    auto readRelocations = [&](std::uint16_t relaIdx, SectionKind kind, std::uint16_t targetSecIdx,
                                std::uint64_t targetSize) -> std::optional<Fail> {
        const Elf64_Shdr &relaHdr = sections[relaIdx].header;
        if (relaHdr.sh_entsize != sizeof(Elf64_Rela)) {
            return Fail{"unexpected relocation entry size"};
        }
        if (relaHdr.sh_info != targetSecIdx) {
            return Fail{"relocation section does not target the expected section"};
        }
        if (relaHdr.sh_size % relaHdr.sh_entsize != 0) {
            return Fail{"relocation table size is not a multiple of its entry size"};
        }
        std::uint64_t count = relaHdr.sh_size / relaHdr.sh_entsize;
        for (std::uint64_t i = 0; i < count; ++i) {
            auto rela = readStruct<Elf64_Rela>(bytes, relaHdr.sh_offset + i * relaHdr.sh_entsize);
            if (!rela) return Fail{"truncated relocation table"};

            std::uint32_t symIndex = elf64RSym(rela->r_info);
            if (symIndex >= object.symbols.size()) {
                return Fail{"relocation references an out-of-range symbol index"};
            }
            std::uint32_t type = elf64RType(rela->r_info);

            RelocationType relType;
            std::uint32_t width;
            if (type == R_X86_64_64) {
                relType = RelocationType::Abs64;
                width = 8;
            } else if (type == R_X86_64_PC32 || type == R_X86_64_PLT32) {
                relType = RelocationType::Pc32;
                width = 4;
            } else {
                return Fail{"relocation type " + std::to_string(type) + " targeting symbol '" +
                            object.symbols[symIndex].name + "' is unsupported"};
            }

            if (rela->r_offset > targetSize || width > targetSize - rela->r_offset) {
                return Fail{"relocation offset is out of range for its section"};
            }

            object.relocations.push_back(Relocation{kind, static_cast<std::uint32_t>(rela->r_offset), symIndex,
                                                      relType, rela->r_addend});
        }
        return std::nullopt;
    };

    if (relaTextIdx) {
        if (!textIdx) return fail(".rela.text present without a .text section");
        if (auto err = readRelocations(*relaTextIdx, SectionKind::Text, *textIdx, object.text.size())) {
            return fail(err->message);
        }
    }
    if (relaDataIdx) {
        if (!dataIdx) return fail(".rela.data present without a .data section");
        if (auto err = readRelocations(*relaDataIdx, SectionKind::Data, *dataIdx, object.data.size())) {
            return fail(err->message);
        }
    }

    return ReadResult{std::move(object), {}};
}

ReadResult readElfObject(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) {
        return ReadResult{std::nullopt, path.string() + ": could not open file"};
    }
    std::streamsize size = in.tellg();
    in.seekg(0);
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    if (size > 0 && !in.read(reinterpret_cast<char *>(bytes.data()), size)) {
        return ReadResult{std::nullopt, path.string() + ": error reading file"};
    }
    return readElfObject(bytes, path.filename().string());
}

} // namespace clnk
