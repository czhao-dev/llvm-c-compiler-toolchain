#include "linker.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

#include "elf_reader.h"
#include "support/compile_fixture.h"

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

std::vector<std::byte> packLE32(std::uint32_t value) {
    std::vector<std::byte> out(4);
    for (int i = 0; i < 4; ++i) out[i] = static_cast<std::byte>((value >> (8 * i)) & 0xff);
    return out;
}

} // namespace

int main() {
    auto scratch = makeScratchDir("linker_test");

    // --- End-to-end success: entry point, call-site relocation, and a
    //     .data relocation with an auto-computed data base, all together. ---
    {
        std::vector<clnk::ObjectFile> files;
        files.push_back(mustRead(compileNamedFixture("start_calls_add", scratch)));   // file 0
        files.push_back(mustRead(compileNamedFixture("math", scratch)));              // file 1
        files.push_back(mustRead(compileNamedFixture("data_pointer", scratch)));      // file 2
        std::size_t startTextSize = files[0].text.size();

        clnk::LinkOptions options;
        options.entrySymbol = "_start";
        options.textBase = 0x401000;
        options.dataBase = 0; // auto

        clnk::LinkResult result = clnk::linkObjects(files, options);
        for (const auto &d : result.diagnostics) std::cerr << clnk::toString(d) << "\n";
        expect(result.ok, "linking start_calls_add.o + math.o + data_pointer.o should succeed");

        expect(result.image.entryPoint == options.textBase, "_start is the first symbol in file 0's .text");
        expect(result.image.dataBase % clnk::kPageSize == 0, "an auto-computed data base must be page-aligned");
        expect(result.image.dataBase >= result.image.textBase + result.image.text.size(),
               "the auto-computed data base must not overlap .text");

        // The call site (file 0, offset 0x13 per the compiled relocation)
        // should have been patched to a Pc32 displacement landing exactly
        // on math's `add`, which starts right after file 0's own .text
        // (16-byte aligned).
        std::size_t addTextOffset = ((startTextSize + 16 - 1) / 16) * 16;
        std::uint64_t addAddress = options.textBase + addTextOffset;
        const clnk::Relocation &callReloc = files[0].relocations[0];
        std::uint64_t P = options.textBase + callReloc.offset;
        std::int32_t expectedDisp = static_cast<std::int32_t>(static_cast<std::int64_t>(addAddress) + callReloc.addend -
                                                                static_cast<std::int64_t>(P));
        expect(std::memcmp(result.image.text.data() + callReloc.offset, packLE32(static_cast<std::uint32_t>(expectedDisp)).data(),
                            4) == 0,
               "the call-site relocation should be patched to reach `add`");
    }

    // --- Undefined symbol ---
    {
        std::vector<clnk::ObjectFile> files;
        files.push_back(mustRead(compileNamedFixture("start_calls_add", scratch)));

        clnk::LinkOptions options;
        clnk::LinkResult result = clnk::linkObjects(files, options);
        expect(!result.ok, "linking start_calls_add.o alone (no math.o) should fail");
        expect(result.diagnostics.size() == 1, "exactly one diagnostic expected");
        expect(result.diagnostics[0].code == clnk::DiagnosticCode::UndefinedSymbol, "should report UndefinedSymbol");
        expect(result.diagnostics[0].symbol == "add", "should name `add`");
    }

    // --- Multiple definition ---
    {
        std::vector<clnk::ObjectFile> files;
        files.push_back(mustRead(compileNamedFixture("math", scratch)));
        files.push_back(mustRead(compileNamedFixture("conflict_add", scratch)));

        clnk::LinkOptions options;
        clnk::LinkResult result = clnk::linkObjects(files, options);
        expect(!result.ok, "linking two definitions of `add` should fail");
        expect(result.diagnostics.size() == 1, "exactly one diagnostic expected");
        expect(result.diagnostics[0].code == clnk::DiagnosticCode::MultipleDefinition, "should report MultipleDefinition");
        expect(result.diagnostics[0].symbol == "add", "should name `add`");
    }

    std::cout << "linker_test: all checks passed\n";
    return 0;
}
