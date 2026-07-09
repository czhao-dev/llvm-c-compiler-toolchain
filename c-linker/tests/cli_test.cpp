#include <sys/wait.h>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "elf64.h"
#include "support/compile_fixture.h"

namespace {

void expect(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::abort();
    }
}

struct CommandResult {
    int exitCode = -1;
    std::string stdOut;
    std::string stdErr;
};

std::string readFileText(const std::filesystem::path &path) {
    std::ifstream in(path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

CommandResult runCommand(const std::string &command, const std::filesystem::path &scratch) {
    auto outPath = scratch / "stdout.txt";
    auto errPath = scratch / "stderr.txt";
    std::string full = command + " >\"" + outPath.string() + "\" 2>\"" + errPath.string() + "\"";
    int status = std::system(full.c_str());

    CommandResult result;
    result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    result.stdOut = readFileText(outPath);
    result.stdErr = readFileText(errPath);
    return result;
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
    auto scratch = makeScratchDir("cli_test");

    auto startObj = compileNamedFixture("start_calls_add", scratch);
    auto mathObj = compileNamedFixture("math", scratch);
    auto conflictObj = compileNamedFixture("conflict_add", scratch);

    // --- Success: link, then read the produced ELF executable back. ---
    {
        auto outPath = scratch / "prog";
        std::string cmd = "\"" CLNK_BINARY "\" \"" + startObj.string() + "\" \"" + mathObj.string() + "\" -o \"" +
                           outPath.string() + "\"";
        CommandResult result = runCommand(cmd, scratch);
        expect(result.exitCode == 0, "a successful link should exit 0, stderr: " + result.stdErr);
        expect(result.stdOut.empty(), "a successful link should print nothing to stdout");

        std::vector<std::byte> bytes = readFileBytes(outPath);
        expect(bytes.size() >= sizeof(clnk::elf::Elf64_Ehdr), "output should at least contain an ELF header");
        clnk::elf::Elf64_Ehdr ehdr;
        std::memcpy(&ehdr, bytes.data(), sizeof(ehdr));
        expect(ehdr.e_ident[0] == clnk::elf::kElfMag0 && ehdr.e_ident[1] == clnk::elf::kElfMag1, "should have ELF magic");
        expect(ehdr.e_type == clnk::elf::ET_EXEC, "output should be an executable (ET_EXEC)");
        expect(ehdr.e_machine == clnk::elf::EM_X86_64, "output should target x86-64");
        expect(ehdr.e_phnum == 2, "output should have exactly two program headers (text, data)");
        expect(ehdr.e_entry == 0x401000, "entry point should be the default text base (_start is file 0's first symbol)");
    }

    // --- Undefined symbol: exit 1, diagnostic on stderr. ---
    {
        auto outPath = scratch / "undef";
        std::string cmd = "\"" CLNK_BINARY "\" \"" + startObj.string() + "\" -o \"" + outPath.string() + "\"";
        CommandResult result = runCommand(cmd, scratch);
        expect(result.exitCode == 1, "linking with an undefined symbol should exit 1");
        expect(result.stdErr.find("add") != std::string::npos, "stderr should mention the undefined symbol");
        expect(!std::filesystem::exists(outPath), "no output file should be written on failure");
    }

    // --- Multiple definition: exit 1. ---
    {
        auto outPath = scratch / "conflict";
        std::string cmd =
            "\"" CLNK_BINARY "\" \"" + mathObj.string() + "\" \"" + conflictObj.string() + "\" -o \"" + outPath.string() + "\"";
        CommandResult result = runCommand(cmd, scratch);
        expect(result.exitCode == 1, "linking two definitions of `add` should exit 1");
        expect(result.stdErr.find("multiple definition") != std::string::npos, "stderr should mention the conflict");
    }

    // --- Usage errors: exit 2. ---
    {
        CommandResult noOutput = runCommand("\"" CLNK_BINARY "\" \"" + mathObj.string() + "\"", scratch);
        expect(noOutput.exitCode == 2, "missing -o should exit 2");

        CommandResult noInputs = runCommand("\"" CLNK_BINARY "\" -o \"" + (scratch / "x").string() + "\"", scratch);
        expect(noInputs.exitCode == 2, "no input files should exit 2");

        CommandResult missingInput =
            runCommand("\"" CLNK_BINARY "\" \"" + (scratch / "does_not_exist.o").string() + "\" -o \"" +
                           (scratch / "x").string() + "\"",
                       scratch);
        expect(missingInput.exitCode == 2, "a nonexistent input path should exit 2");
    }

    // --- Help: exit 0, usage on stdout. ---
    {
        CommandResult help = runCommand(std::string("\"") + CLNK_BINARY + "\" --help", scratch);
        expect(help.exitCode == 0, "--help should exit 0");
        expect(help.stdOut.find("usage") != std::string::npos, "--help should print usage text");
    }

    std::cout << "cli_test: all checks passed\n";
    return 0;
}
