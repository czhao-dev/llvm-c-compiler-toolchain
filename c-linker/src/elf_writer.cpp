#include "elf_writer.h"

#include "elf64.h"

namespace clnk {

using namespace elf;

namespace {

std::uint64_t alignUp(std::uint64_t value, std::uint64_t align) {
    return ((value + align - 1) / align) * align;
}

template <typename T>
void appendStruct(std::vector<std::byte> &out, const T &value) {
    const auto *bytes = reinterpret_cast<const std::byte *>(&value);
    out.insert(out.end(), bytes, bytes + sizeof(T));
}

} // namespace

std::vector<std::byte> writeElfExecutable(const LinkedImage &image) {
    constexpr std::uint64_t kHeaderBytes = sizeof(Elf64_Ehdr) + 2 * sizeof(Elf64_Phdr);
    static_assert(kHeaderBytes <= kPageSize, "ELF header + program headers must fit in the reserved header page");

    Elf64_Ehdr ehdr{};
    ehdr.e_ident[0] = kElfMag0;
    ehdr.e_ident[1] = kElfMag1;
    ehdr.e_ident[2] = kElfMag2;
    ehdr.e_ident[3] = kElfMag3;
    ehdr.e_ident[EI_CLASS] = ELFCLASS64;
    ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
    ehdr.e_ident[EI_VERSION] = EV_CURRENT;
    ehdr.e_type = ET_EXEC;
    ehdr.e_machine = EM_X86_64;
    ehdr.e_version = EV_CURRENT;
    ehdr.e_entry = image.entryPoint;
    ehdr.e_phoff = sizeof(Elf64_Ehdr);
    ehdr.e_shoff = 0;
    ehdr.e_flags = 0;
    ehdr.e_ehsize = sizeof(Elf64_Ehdr);
    ehdr.e_phentsize = sizeof(Elf64_Phdr);
    ehdr.e_phnum = 2;
    ehdr.e_shentsize = 0;
    ehdr.e_shnum = 0;
    ehdr.e_shstrndx = 0;

    const std::uint64_t textFileOffset = kPageSize;
    const std::uint64_t dataFileOffset = alignUp(textFileOffset + image.text.size(), kPageSize);

    Elf64_Phdr textPhdr{};
    textPhdr.p_type = PT_LOAD;
    textPhdr.p_flags = PF_R | PF_X;
    textPhdr.p_offset = 0;
    textPhdr.p_vaddr = image.textBase - kPageSize;
    textPhdr.p_paddr = textPhdr.p_vaddr;
    textPhdr.p_filesz = textFileOffset + image.text.size();
    textPhdr.p_memsz = textPhdr.p_filesz;
    textPhdr.p_align = kPageSize;

    Elf64_Phdr dataPhdr{};
    dataPhdr.p_type = PT_LOAD;
    dataPhdr.p_flags = PF_R | PF_W;
    dataPhdr.p_offset = dataFileOffset;
    dataPhdr.p_vaddr = image.dataBase;
    dataPhdr.p_paddr = image.dataBase;
    dataPhdr.p_filesz = image.data.size();
    dataPhdr.p_memsz = image.data.size();
    dataPhdr.p_align = kPageSize;

    std::vector<std::byte> out;
    out.reserve(dataFileOffset + image.data.size());

    appendStruct(out, ehdr);
    appendStruct(out, textPhdr);
    appendStruct(out, dataPhdr);
    out.resize(kPageSize, std::byte{0}); // pad out the reserved header page

    out.insert(out.end(), image.text.begin(), image.text.end());
    out.resize(dataFileOffset, std::byte{0}); // pad up to .data's page boundary
    out.insert(out.end(), image.data.begin(), image.data.end());

    return out;
}

} // namespace clnk
