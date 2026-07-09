#ifndef SA_CLI_H
#define SA_CLI_H

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "config.h"

namespace sa {

struct ScanArgs {
    std::vector<std::string> paths;
    std::optional<std::int64_t> maxComplexity;
    std::optional<std::int64_t> maxNesting;
    std::optional<std::string> select;
    std::vector<std::string> exclude;
    bool noConfig = false;
    bool showHelp = false;
    bool showSource = false;
};

// Thrown for a malformed CLI invocation (unrecognized subcommand, unknown
// flag, or a flag missing its required value). what() is the exact error
// text to print; always maps to exit code 2.
class CliError : public std::runtime_error {
public:
    explicit CliError(const std::string &message) : std::runtime_error(message) {}
};

// Parses argv[1..] (excludes the program name). Expects a literal "scan"
// subcommand as the first token (mirroring the original clap-derived CLI's
// only subcommand), followed by scan's own flags/positionals.
ScanArgs parseArgs(const std::vector<std::string> &args);

// Applies config-file loading (unless noConfig) plus CLI overrides, in the
// same precedence order as the original tool: max-complexity, max-nesting,
// select (replaces enabledRules wholesale), exclude (extends).
Config buildConfig(const ScanArgs &args);

// Runs the "scan" subcommand end to end and returns the process exit code
// (0 clean, 1 diagnostics found, 2 a missing path).
int runScan(const ScanArgs &args);

// Parses argv and runs the whole CLI; returns the process exit code.
int run(int argc, char **argv);

} // namespace sa

#endif // SA_CLI_H
