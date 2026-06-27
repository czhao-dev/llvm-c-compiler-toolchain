#ifndef BT_CLI_H
#define BT_CLI_H

#include <stdexcept>
#include <string>
#include <vector>

namespace bt {

// Parsed command-line arguments for the build-tool binary.
struct Args {
    std::vector<std::string> targets;
    bool keepGoing = false;
};

// Thrown by parseArgs() for a malformed or unsupported flag; what() is the
// exact usage-error text to print.
class CliError : public std::runtime_error {
public:
    explicit CliError(const std::string &message) : std::runtime_error(message) {}
};

// Parses argv[1..] (excludes the program name). `-k`/`--keep-going` sets
// keepGoing. `-j`/`-n`/`-f` are recognized but rejected as not-yet-supported
// (a clear error, rather than being silently misread as target names).
// Any other `-`-prefixed argument (other than a bare "-") is an unknown
// flag; everything else is a target.
Args parseArgs(const std::vector<std::string> &args);

} // namespace bt

#endif // BT_CLI_H
