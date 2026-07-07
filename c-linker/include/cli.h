#ifndef CLNK_CLI_H
#define CLNK_CLI_H

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace clnk {

struct LinkArgs {
    std::vector<std::string> inputs;
    std::string output;
    std::string entry = "_start";
    std::uint64_t textBase = 0x401000;
    std::uint64_t dataBase = 0; // 0 = auto
    bool showHelp = false;
};

// Thrown for a malformed CLI invocation (unrecognized flag, a flag missing
// its required value, an unparseable numeric argument, no input files, a
// missing -o/--output, or an input path that doesn't exist on disk).
// what() is the exact error text to print; always maps to exit code 2.
class CliError : public std::runtime_error {
public:
    explicit CliError(const std::string &message) : std::runtime_error(message) {}
};

// Parses argv[1..] (excludes the program name).
LinkArgs parseArgs(const std::vector<std::string> &args);

// Runs the link end to end (reads inputs, links, writes the output
// executable with the executable permission bit set) and returns the
// process exit code: 0 linked successfully, 1 the link failed (diagnostics
// printed to stderr).
int runLink(const LinkArgs &args);

// Parses argv and runs the whole CLI; returns the process exit code.
int run(int argc, char **argv);

} // namespace clnk

#endif // CLNK_CLI_H
