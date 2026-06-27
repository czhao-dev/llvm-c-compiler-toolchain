#ifndef BT_MAKEFILE_H
#define BT_MAKEFILE_H

#include <filesystem>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace bt {

// A single explicit rule: one target, its prerequisites, and the shell
// commands that rebuild it.
struct Rule {
    std::string target;
    std::vector<std::string> prereqs;
    std::vector<std::string> recipe;
    bool phony = false;
};

// The result of parsing a Makefile: its rules in source order (used to pick
// the default goal) plus the set of names declared `.PHONY`.
struct ParsedMakefile {
    std::vector<Rule> rules;
    std::set<std::string> phonyTargets;

    // The first rule whose target doesn't start with '.', used as the
    // default goal when none is given on the command line.
    std::optional<std::string> defaultGoal() const;

    // Looks up the rule for a given target name, if one exists.
    const Rule *ruleFor(const std::string &target) const;
};

// Thrown by parseMakefile() with a message of the form "{line}: <reason>".
class MakefileParseError : public std::runtime_error {
public:
    explicit MakefileParseError(const std::string &message) : std::runtime_error(message) {}
};

// Parses Makefile text into rules and `.PHONY` declarations.
//
// Lines are processed as follows:
// - Blank lines and `#`-comment lines are skipped.
// - A line starting with a tab is a recipe line, attached to the most
//   recently started rule header.
// - A line of the form `.PHONY: a b c` records `a`, `b`, `c` as phony
//   target names (applied to matching rules after the full parse).
// - Any other non-blank line is a rule header `target: prereq prereq...`.
ParsedMakefile parseMakefile(const std::string &text);

// Looks for `Makefile` then `makefile` inside `dir`, returning the first
// one found.
std::optional<std::filesystem::path> discoverMakefile(const std::filesystem::path &dir);

} // namespace bt

#endif // BT_MAKEFILE_H
