#include "cli.h"

#include <cstdlib>
#include <iostream>

#include "CLI11.hpp"

namespace bt {

Args parseArgs(const std::vector<std::string> &args) {
    CLI::App app{"build-tool"};

    Args result;
    app.add_flag("-k,--keep-going", result.keepGoing, "keep going after a target fails");

    // `-n`/`-f`/`-j` are recognized (so they're never silently misread as
    // target names) but rejected as not-yet-supported. `-j` additionally
    // takes an optional attached value (`-j4`, matching real Make's
    // parallelism flag) purely so that form is recognized too, not because
    // the value is ever used.
    bool dryRun = false;
    bool makefileOverride = false;
    std::string jobsValue;
    app.add_flag("-n", dryRun);
    app.add_flag("-f", makefileOverride);
    app.add_option("-j", jobsValue)->expected(0, 1);

    // Positional arguments are build targets; CLI11 collects everything
    // not matched as a flag/option here in order.
    app.add_option("targets", result.targets, "targets to build");

    try {
        // CLI11's vector<string> overload consumes the vector like a stack
        // (from the back), so it expects arguments in reverse order.
        std::vector<std::string> reversed(args.rbegin(), args.rend());
        app.parse(reversed);
    } catch (const CLI::CallForHelp &) {
        // CLI11's auto-added -h/--help throws this as control flow rather
        // than returning a value; print the generated help text and exit
        // successfully, matching the spec's expectation that a CLI library
        // handles --help "automatically" rather than as a usage error.
        std::cout << app.help();
        std::exit(0);
    } catch (const CLI::ParseError &e) {
        throw CliError(e.what());
    }

    if (dryRun) throw CliError("'-n' is not yet supported");
    if (makefileOverride) throw CliError("'-f' is not yet supported");
    if (app.count("-j") > 0) throw CliError("'-j' is not yet supported");

    return result;
}

} // namespace bt
