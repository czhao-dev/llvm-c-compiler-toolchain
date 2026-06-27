#ifndef BT_PLANNER_H
#define BT_PLANNER_H

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "makefile.h"

namespace bt {

// The outcome of evaluating a single plan node (one Makefile target).
enum class BuildStatus {
    UpToDate, // the target's file is newer than all of its prerequisites; nothing was run
    Built,    // the recipe ran and every command succeeded
    Failed,   // the recipe ran and a command exited non-zero (or couldn't launch)
    Skipped,  // not attempted, because a prerequisite failed (or the run already failed
              // elsewhere and keep-going wasn't requested)
};

// Thrown by plan() for an error discovered while resolving rule names into
// a dependency graph, before any recipe has run. Always fatal, regardless
// of keep-going.
class PlanError : public std::runtime_error {
public:
    explicit PlanError(const std::string &message) : std::runtime_error(message) {}
};

// One resolved node in a topologically-ordered build plan.
struct PlanNode {
    std::string target;
    std::vector<std::string> recipe;
    bool phony = false;
    // True for a prerequisite that has no rule of its own but exists on
    // disk (a source file). Such a node always evaluates to UpToDate,
    // regardless of prereqNames/prereqIndices (both empty for a leaf).
    bool isLeaf = false;
    std::vector<std::string> prereqNames;      // raw names, for mtime staleness checks
    std::vector<std::size_t> prereqIndices;    // indices into the plan's node list; always < this node's own index
};

// A fully-resolved build plan: nodes in topological order (a node's
// prerequisites always precede it), plus the node index of each requested
// goal, in the same order the goals were given.
struct Plan {
    std::vector<PlanNode> nodes;
    std::vector<std::size_t> goalIndices;
};

// Resolves `goals` (and everything they transitively depend on) against
// `rules` into a topologically-ordered Plan. Throws PlanError on an
// unresolvable target name (`NoRule`-equivalent message) or a circular
// dependency (`Cycle`-equivalent message).
Plan plan(const ParsedMakefile &rules, const std::vector<std::string> &goals);

// Executes a Plan's nodes in order, on the calling thread, and returns one
// BuildStatus per node (indices matching Plan::nodes). Recipe lines are run
// as `sh -c <line>`, echoing the line to stdout before running it.
std::vector<BuildStatus> execute(const Plan &builtPlan, bool keepGoing);

} // namespace bt

#endif // BT_PLANNER_H
