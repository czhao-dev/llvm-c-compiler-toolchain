#include "elf_reader.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>

#include "support/compile_fixture.h"

namespace {

void expect(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::abort();
    }
}

std::vector<std::byte> readFileBytes(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    std::streamsize size = in.tellg();
    in.seekg(0);
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    in.read(reinterpret_cast<char *>(bytes.data()), size);
    return bytes;
}

} // namespace

int main() {
    auto scratch = makeScratchDir("elf_reader_test");
    auto mathObj = compileNamedFixture("math", scratch);

    clnk::ReadResult result = clnk::readElfObject(mathObj);
    expect(static_cast<bool>(result), "math.o should parse successfully: " + result.error);
    const clnk::ObjectFile &object = *result.file;

    expect(!object.text.empty(), "math.o should have non-empty .text");
    expect(object.data.empty(), "math.o should have no .data");
    expect(object.relocations.empty(), "math.o should have no relocations (add doesn't call anything)");

    bool foundAdd = false;
    for (const clnk::Symbol &sym : object.symbols) {
        if (sym.name == "add") {
            foundAdd = true;
            expect(sym.binding == clnk::SymbolBinding::Global, "add should be a global symbol");
            expect(sym.location == clnk::SymbolLocation::Text, "add should be defined in .text");
            expect(sym.value == 0, "add should be at offset 0 within .text");
        }
    }
    expect(foundAdd, "math.o's symbol table should contain `add`");

    // Malformed input: bad magic.
    {
        std::vector<std::byte> bytes = readFileBytes(mathObj);
        bytes[0] = std::byte{0x00};
        clnk::ReadResult bad = clnk::readElfObject(bytes, "corrupt.o");
        expect(!bad, "flipping the ELF magic byte should fail to parse");
    }

    // Malformed input: truncated file.
    {
        std::vector<std::byte> bytes = readFileBytes(mathObj);
        bytes.resize(10);
        clnk::ReadResult bad = clnk::readElfObject(bytes, "truncated.o");
        expect(!bad, "a truncated file should fail to parse");
    }

    // Malformed input: corrupted section header offset (e_shoff is the
    // 8-byte little-endian field at byte offset 0x28 in Elf64_Ehdr).
    {
        std::vector<std::byte> bytes = readFileBytes(mathObj);
        for (int i = 0; i < 8; ++i) {
            bytes[0x28 + i] = std::byte{0xff};
        }
        clnk::ReadResult bad = clnk::readElfObject(bytes, "bad-shoff.o");
        expect(!bad, "a corrupted section header offset should fail to parse, not crash");
    }

    std::cout << "elf_reader_test: all checks passed\n";
    return 0;
}
