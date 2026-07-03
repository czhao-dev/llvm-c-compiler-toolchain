// `build-tool`: a dependency-graph build tool implementing core GNU Make
// semantics (rules, prerequisites, recipes, `.PHONY`, timestamp-based
// rebuilds), executed serially in topological order. See the README for
// supported / not-yet-supported features.

#include "cli.h"
#include "makefile.h"
#include "planner.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

int usageError(const std::exception &e) {
    std::cerr << "build-tool: " << e.what() << "\n";
    return 2;
}

// Discovers + parses the makefile, plans, executes, and returns a process
// exit code: 0 success, 1 a goal failed (or was skipped due to a failed
// dependency), 2 a usage/parse/plan error.
int run(const bt::Args &args, const std::filesystem::path &cwd) {
    std::optional<std::filesystem::path> makefilePath = bt::discoverMakefile(cwd);
    if (!makefilePath.has_value()) {
        std::cerr << "build-tool: no makefile found in " << cwd.string() << "\n";
        return 2;
    }

    std::ifstream in(*makefilePath, std::ios::binary);
    if (!in) {
        std::cerr << "build-tool: failed to read " << makefilePath->string() << "\n";
        return 2;
    }
    std::ostringstream contents;
    contents << in.rdbuf();

    bt::ParsedMakefile parsed;
    try {
        parsed = bt::parseMakefile(contents.str());
    } catch (const std::exception &e) {
        std::cerr << "build-tool: " << makefilePath->string() << ": " << e.what() << "\n";
        return 2;
    }

    std::vector<std::string> goals;
    if (!args.targets.empty()) {
        goals = args.targets;
    } else if (std::optional<std::string> goal = parsed.defaultGoal()) {
        goals = {*goal};
    } else {
        std::cerr << "build-tool: no targets specified and no default goal in makefile\n";
        return 2;
    }

    bt::Plan builtPlan;
    try {
        builtPlan = bt::plan(parsed, goals);
    } catch (const std::exception &e) {
        std::cerr << "build-tool: " << e.what() << "\n";
        return 2;
    }

    std::vector<bt::BuildStatus> statuses = bt::execute(builtPlan, args.keepGoing);

    bool anyFailed = false;
    for (std::size_t i = 0; i < goals.size(); ++i) {
        const std::string &goal = goals[i];
        switch (statuses[builtPlan.goalIndices[i]]) {
            case bt::BuildStatus::UpToDate:
                std::cout << "build-tool: '" << goal << "' is up to date.\n";
                break;
            case bt::BuildStatus::Built:
                break;
            case bt::BuildStatus::Failed:
                std::cerr << "build-tool: *** [" << goal << "] failed\n";
                anyFailed = true;
                break;
            case bt::BuildStatus::Skipped:
                std::cerr << "build-tool: *** [" << goal << "] skipped due to a failed dependency\n";
                anyFailed = true;
                break;
        }
    }

    return anyFailed ? 1 : 0;
}

} // namespace

int main(int argc, char **argv) {
    std::vector<std::string> args(argv + 1, argv + argc);

    bt::Args parsedArgs;
    try {
        parsedArgs = bt::parseArgs(args);
    } catch (const std::exception &e) {
        return usageError(e);
    }

    return run(parsedArgs, std::filesystem::current_path());
}
