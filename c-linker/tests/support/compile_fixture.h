#ifndef CLNK_TESTS_COMPILE_FIXTURE_H
#define CLNK_TESTS_COMPILE_FIXTURE_H

#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>

// Shells out to the real clang discovered at configure time (CLNK_CLANG)
// to compile a fixture .c source into a real ELF64 x86-64 relocatable
// object file -- this project's tests exercise the linker against real
// compiler output, not hand-assembled bytes. `--target=x86_64-unknown-linux-gnu`
// lets this run identically on macOS and Linux dev/CI machines without a
// Linux sysroot, since fixtures never #include any system header.
// -fno-pic keeps relocations to the small set this linker supports
// (R_X86_64_64 / R_X86_64_PC32 / R_X86_64_PLT32); -fno-function-sections
// guarantees exactly one .text section per file.
inline std::filesystem::path compileFixture(const std::filesystem::path &sourcePath,
                                             const std::filesystem::path &outputPath) {
    std::ostringstream command;
    command << "\"" << CLNK_CLANG << "\""
            << " --target=x86_64-unknown-linux-gnu -fno-pic -fno-function-sections"
            << " -c \"" << sourcePath.string() << "\""
            << " -o \"" << outputPath.string() << "\"";
    if (std::system(command.str().c_str()) != 0) {
        throw std::runtime_error("failed to compile fixture: " + sourcePath.string());
    }
    return outputPath;
}

// Compiles tests/fixtures/<name>.c (CLNK_FIXTURES_DIR) into <scratchDir>/<name>.o.
inline std::filesystem::path compileNamedFixture(const std::string &name, const std::filesystem::path &scratchDir) {
    std::filesystem::path source = std::filesystem::path(CLNK_FIXTURES_DIR) / (name + ".c");
    std::filesystem::path output = scratchDir / (name + ".o");
    return compileFixture(source, output);
}

// A fresh, process-unique scratch directory under the system temp dir, for
// compiled fixtures and linker output.
inline std::filesystem::path makeScratchDir(const std::string &label) {
    auto dir = std::filesystem::temp_directory_path() /
               ("clnk_" + label + "_" + std::to_string(static_cast<long>(::getpid())));
    std::filesystem::create_directories(dir);
    return dir;
}

#endif // CLNK_TESTS_COMPILE_FIXTURE_H
