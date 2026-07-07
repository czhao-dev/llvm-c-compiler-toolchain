#include "section_merger.h"

#include <cstdlib>
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

bool allZero(const std::vector<std::byte> &bytes, std::size_t begin, std::size_t end) {
    for (std::size_t i = begin; i < end; ++i) {
        if (bytes[i] != std::byte{0}) return false;
    }
    return true;
}

} // namespace

int main() {
    auto scratch = makeScratchDir("section_merger_test");

    clnk::ObjectFile math = mustRead(compileNamedFixture("math", scratch));
    clnk::ObjectFile helperA = mustRead(compileNamedFixture("local_helper_a", scratch));
    clnk::ObjectFile dataOnly = mustRead(compileNamedFixture("data_pointer", scratch));

    expect(math.textAlign == 16, "math.o's .text alignment should be 16 (sanity check on the fixture)");
    expect(helperA.textAlign == 16, "local_helper_a.o's .text alignment should be 16 (sanity check on the fixture)");

    std::vector<clnk::ObjectFile> files;
    files.push_back(math);
    files.push_back(helperA);
    files.push_back(dataOnly);

    clnk::MergedLayout layout = clnk::mergeSections(files, 0x1000, 0x2000);

    expect(layout.textFileOffset[0].has_value() && *layout.textFileOffset[0] == 0,
           "math's .text should start at merged offset 0");

    std::size_t mathTextSize = math.text.size();
    std::size_t expectedHelperOffset = ((mathTextSize + 16 - 1) / 16) * 16;
    expect(layout.textFileOffset[1].has_value() && *layout.textFileOffset[1] == expectedHelperOffset,
           "local_helper_a's .text should start at the next 16-byte-aligned offset after math's");
    expect(allZero(layout.text, mathTextSize, expectedHelperOffset), "inter-file padding bytes should be zero");

    expect(!layout.textFileOffset[2].has_value(), "data_pointer.o contributes no .text");
    expect(!layout.dataFileOffset[0].has_value(), "math.o contributes no .data");
    expect(!layout.dataFileOffset[1].has_value(), "local_helper_a.o contributes no .data");
    expect(layout.dataFileOffset[2].has_value() && *layout.dataFileOffset[2] == 0,
           "data_pointer's .data should start at merged offset 0");

    expect(layout.text.size() == expectedHelperOffset + helperA.text.size(), "merged .text size should match");
    expect(layout.data.size() == dataOnly.data.size(), "merged .data size should match");

    std::cout << "section_merger_test: all checks passed\n";
    return 0;
}
