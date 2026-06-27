#include "cli.h"

namespace bt {

namespace {

bool startsWith(const std::string &s, const std::string &prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

} // namespace

Args parseArgs(const std::vector<std::string> &args) {
    Args result;

    for (const std::string &arg : args) {
        if (arg == "-n" || arg == "-f") {
            throw CliError("'" + arg + "' is not yet supported");
        } else if (arg == "-k" || arg == "--keep-going") {
            result.keepGoing = true;
        } else if (arg == "-j" || startsWith(arg, "-j")) {
            throw CliError("'" + arg + "' is not yet supported");
        } else if (!arg.empty() && arg.front() == '-' && arg != "-") {
            throw CliError("unknown flag '" + arg + "'");
        } else {
            result.targets.push_back(arg);
        }
    }

    return result;
}

} // namespace bt
