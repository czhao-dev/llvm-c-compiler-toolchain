#include "relocation_applier.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include "elf_reader.h"
#include "section_merger.h"
#include "support/compile_fixture.h"
#include "symbol_resolver.h"

namespace {

void expect(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::abort();
    }
}

clnk::ObjectFile mustRead(const std::filesystem::path &path) {
    clnk::ReadResult result = clnk::readElfObject(path);
    expect(static_cast<bool>(result), path.string() + ": " + result.error);
    return std::move(*result.file);
}

// Independent (not shared with relocation_applier.cpp) little-endian byte
// packer, so this test doesn't just re-run the same helper it's supposed
// to be checking.
std::vector<std::byte> packLE(std::uint64_t value, unsigned width) {
    std::vector<std::byte> out(width);
    for (unsigned i = 0; i < width; ++i) {
        out[i] = static_cast<std::byte>((value >> (8 * i)) & 0xff);
    }
    return out;
}

bool rangeEquals(const std::vector<std::byte> &bytes, std::size_t offset, const std::vector<std::byte> &expected) {
    if (offset + expected.size() > bytes.size()) return false;
    return std::memcmp(bytes.data() + offset, expected.data(), expected.size()) == 0;
}

} // namespace

int main() {
    auto scratch = makeScratchDir("relocation_test");

    // --- Abs64: a .data pointer to another global ---
    {
        std::vector<clnk::ObjectFile> files;
        files.push_back(mustRead(compileNamedFixture("data_pointer", scratch)));

        std::vector<clnk::Diagnostic> diagnostics;
        clnk::SymbolTable table = clnk::buildSymbolTable(files, diagnostics);
        expect(diagnostics.empty(), "data_pointer.o should have no symbol diagnostics");

        constexpr std::uint64_t textBase = 0x1000, dataBase = 0x2000;
        clnk::MergedLayout layout = clnk::mergeSections(files, textBase, dataBase);
        clnk::applyRelocations(files, table, layout, diagnostics);
        expect(diagnostics.empty(), "applying the Abs64 relocation should not produce diagnostics");

        // ptr_to_staged holds the address of staged_value; find both
        // symbols and the one relocation targeting ptr_to_staged's slot.
        const clnk::Relocation *reloc = nullptr;
        for (const auto &r : files[0].relocations) {
            if (r.section == clnk::SectionKind::Data) reloc = &r;
        }
        expect(reloc != nullptr, "data_pointer.o should have exactly one .data relocation");
        expect(reloc->type == clnk::RelocationType::Abs64, "the pointer-to-global relocation should be Abs64");

        auto stagedIt = table.definitions.find("staged_value");
        expect(stagedIt != table.definitions.end(), "staged_value should be defined");
        std::uint64_t expectedS = clnk::resolvedAddress(stagedIt->second, layout);
        std::uint64_t expectedValue = expectedS + static_cast<std::uint64_t>(reloc->addend);

        std::size_t patchOffset = *layout.dataFileOffset[0] + reloc->offset;
        expect(rangeEquals(layout.data, patchOffset, packLE(expectedValue, 8)),
               "Abs64 patch should equal little-endian (S + A) over 8 bytes");
    }

    // --- Pc32/Plt32: a call to an external function ---
    {
        std::vector<clnk::ObjectFile> files;
        files.push_back(mustRead(compileNamedFixture("start_calls_add", scratch))); // file 0
        files.push_back(mustRead(compileNamedFixture("math", scratch)));            // file 1

        std::vector<clnk::Diagnostic> diagnostics;
        clnk::SymbolTable table = clnk::buildSymbolTable(files, diagnostics);
        expect(diagnostics.empty(), "no symbol diagnostics expected");
        bool undefinedOk = clnk::checkUndefinedSymbols(files, table, diagnostics);
        expect(undefinedOk, "add is defined in math.o, so there should be no undefined symbols");

        constexpr std::uint64_t textBase = 0x1000, dataBase = 0x2000;
        clnk::MergedLayout layout = clnk::mergeSections(files, textBase, dataBase);
        clnk::applyRelocations(files, table, layout, diagnostics);
        expect(diagnostics.empty(), "applying the Pc32 relocation should not produce diagnostics");

        expect(files[0].relocations.size() == 1, "start_calls_add.o should have exactly one relocation");
        const clnk::Relocation &reloc = files[0].relocations[0];
        expect(reloc.type == clnk::RelocationType::Pc32, "the call to add() should be a Pc32/Plt32 relocation");

        auto addIt = table.definitions.find("add");
        expect(addIt != table.definitions.end(), "add should be defined");
        std::uint64_t S = clnk::resolvedAddress(addIt->second, layout);
        std::uint64_t P = textBase + *layout.textFileOffset[0] + reloc.offset;
        std::int64_t signedResult = static_cast<std::int64_t>(S) + reloc.addend - static_cast<std::int64_t>(P);
        expect(signedResult >= INT32_MIN && signedResult <= INT32_MAX, "this case must fit in 32 bits by construction");
        std::uint32_t expectedValue = static_cast<std::uint32_t>(static_cast<std::int32_t>(signedResult));

        std::size_t patchOffset = *layout.textFileOffset[0] + reloc.offset;
        expect(rangeEquals(layout.text, patchOffset, packLE(expectedValue, 4)),
               "Pc32 patch should equal little-endian (S + A - P) over 4 bytes");
    }

    // --- RelocationOverflow: a Pc32 result too far to fit in 32 bits ---
    {
        clnk::ObjectFile start = mustRead(compileNamedFixture("start_calls_add", scratch));
        std::vector<clnk::ObjectFile> files;
        files.push_back(start);

        expect(files[0].relocations.size() == 1, "sanity: exactly one relocation");
        const clnk::Relocation &reloc = files[0].relocations[0];

        // Fabricate a symbol table where `add` lives absurdly far away, so
        // S - P can't possibly fit in a signed 32-bit displacement.
        clnk::SymbolTable table;
        table.definitions["add"] = clnk::ResolvedSymbol{"add", /*owningFileIndex=*/1, clnk::SectionKind::Text, 0};

        clnk::MergedLayout layout;
        layout.textBase = 0;
        layout.text = files[0].text;
        layout.textFileOffset = {0, 0x1000000000ULL}; // index 0: this file; index 1: `add`'s fictitious file
        layout.dataFileOffset = {std::nullopt};

        std::vector<std::byte> before = layout.text;
        std::vector<clnk::Diagnostic> diagnostics;
        clnk::applyRelocations(files, table, layout, diagnostics);

        expect(diagnostics.size() == 1, "an out-of-range Pc32 result should produce exactly one diagnostic");
        expect(diagnostics[0].code == clnk::DiagnosticCode::RelocationOverflow, "diagnostic should be RelocationOverflow");
        expect(rangeEquals(layout.text, reloc.offset, {before.begin() + reloc.offset, before.begin() + reloc.offset + 4}),
               "an overflowing relocation must leave the original bytes untouched");
    }

    std::cout << "relocation_test: all checks passed\n";
    return 0;
}
