#include "planner.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <system_error>
#include <unordered_map>

#include <sys/wait.h>
#include <unistd.h>

namespace bt {

namespace {

// Walks goal (and rule-prerequisite) names into a flat, topologically
// ordered node list: a name's prerequisites are always resolved (and thus
// appended) before the name itself, so no separate topological sort pass
// is needed afterward. Diamond dependencies are memoized to a single node;
// an in-progress "visiting" stack detects cycles.
class Resolver {
public:
    explicit Resolver(const ParsedMakefile &rules) {
        for (const Rule &rule : rules.rules) {
            ruleByTarget_.emplace(rule.target, &rule); // first rule for a target wins
        }
    }

    std::size_t resolve(const std::string &name, const std::string *neededBy) {
        auto visitingIt = std::find(visiting_.begin(), visiting_.end(), name);
        if (visitingIt != visiting_.end()) {
            std::vector<std::string> chain(visitingIt, visiting_.end());
            chain.push_back(name);
            std::ostringstream oss;
            oss << "Circular dependency dropped: ";
            for (std::size_t i = 0; i < chain.size(); ++i) {
                if (i > 0) oss << " -> ";
                oss << chain[i];
            }
            throw PlanError(oss.str());
        }

        auto memoIt = memo_.find(name);
        if (memoIt != memo_.end()) return memoIt->second;

        visiting_.push_back(name);
        std::size_t index;
        try {
            index = resolveUncached(name, neededBy);
        } catch (...) {
            visiting_.pop_back();
            throw;
        }
        visiting_.pop_back();

        memo_.emplace(name, index);
        return index;
    }

    std::vector<PlanNode> takeNodes() { return std::move(nodes_); }

private:
    std::size_t resolveUncached(const std::string &name, const std::string *neededBy) {
        auto ruleIt = ruleByTarget_.find(name);
        if (ruleIt == ruleByTarget_.end()) {
            std::error_code ec;
            if (std::filesystem::is_regular_file(name, ec)) {
                PlanNode leaf;
                leaf.target = name;
                leaf.isLeaf = true;
                nodes_.push_back(std::move(leaf));
                return nodes_.size() - 1;
            }
            std::string message = "No rule to make target '" + name + "'";
            if (neededBy != nullptr) message += ", needed by '" + *neededBy + "'";
            throw PlanError(message);
        }

        const Rule &rule = *ruleIt->second;
        std::vector<std::size_t> prereqIndices;
        prereqIndices.reserve(rule.prereqs.size());
        for (const std::string &prereq : rule.prereqs) {
            prereqIndices.push_back(resolve(prereq, &name));
        }

        PlanNode node;
        node.target = rule.target;
        node.recipe = rule.recipe;
        node.phony = rule.phony;
        node.prereqNames = rule.prereqs;
        node.prereqIndices = std::move(prereqIndices);
        nodes_.push_back(std::move(node));
        return nodes_.size() - 1;
    }

    std::unordered_map<std::string, const Rule *> ruleByTarget_;
    std::unordered_map<std::string, std::size_t> memo_;
    std::vector<std::string> visiting_;
    std::vector<PlanNode> nodes_;
};

// Computed at execution time (not plan time): a target is stale if it's
// missing, or any prerequisite file is newer than it. Metadata errors (e.g.
// a prerequisite that vanished) are treated as "stale" rather than
// propagated as an error.
bool isStale(const std::string &target, const std::vector<std::string> &prereqNames) {
    std::error_code ec;
    auto targetTime = std::filesystem::last_write_time(target, ec);
    if (ec) return true;

    for (const std::string &prereq : prereqNames) {
        std::error_code prereqEc;
        auto prereqTime = std::filesystem::last_write_time(prereq, prereqEc);
        if (prereqEc) return true;
        if (prereqTime > targetTime) return true;
    }
    return false;
}

// Runs `sh -c line` as a direct child process (no extra shell wrapping),
// mirroring `Command::new("sh").arg("-c").arg(line).status()`. Returns
// true iff the process launched and exited zero.
bool runRecipeLine(const std::string &line) {
    pid_t pid = fork();
    if (pid < 0) return false; // fork failed
    if (pid == 0) {
        execlp("sh", "sh", "-c", line.c_str(), static_cast<char *>(nullptr));
        _exit(127); // exec failed
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return false;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

} // namespace

Plan plan(const ParsedMakefile &rules, const std::vector<std::string> &goals) {
    Resolver resolver(rules);

    std::vector<std::size_t> goalIndices;
    goalIndices.reserve(goals.size());
    for (const std::string &goal : goals) {
        goalIndices.push_back(resolver.resolve(goal, nullptr));
    }

    Plan result;
    result.nodes = resolver.takeNodes();
    result.goalIndices = std::move(goalIndices);
    return result;
}

std::vector<BuildStatus> execute(const Plan &builtPlan, bool keepGoing) {
    std::vector<BuildStatus> statuses(builtPlan.nodes.size());
    bool failFlag = false;

    for (std::size_t i = 0; i < builtPlan.nodes.size(); ++i) {
        const PlanNode &node = builtPlan.nodes[i];

        if (node.isLeaf) {
            statuses[i] = BuildStatus::UpToDate;
            continue;
        }

        if (failFlag && !keepGoing) {
            statuses[i] = BuildStatus::Skipped;
            continue;
        }

        bool anyPrereqFailed = false;
        bool anyPrereqBuilt = false;
        for (std::size_t prereqIdx : node.prereqIndices) {
            switch (statuses[prereqIdx]) {
                case BuildStatus::Failed:
                case BuildStatus::Skipped:
                    anyPrereqFailed = true;
                    break;
                case BuildStatus::Built:
                    anyPrereqBuilt = true;
                    break;
                case BuildStatus::UpToDate:
                    break;
            }
        }

        if (anyPrereqFailed) {
            if (!keepGoing) failFlag = true;
            statuses[i] = BuildStatus::Skipped;
            continue;
        }

        bool stale = node.phony || anyPrereqBuilt || isStale(node.target, node.prereqNames);
        if (!stale) {
            statuses[i] = BuildStatus::UpToDate;
            continue;
        }

        bool recipeFailed = false;
        for (const std::string &line : node.recipe) {
            // Flush before running the command: stdout is fully buffered
            // (not line-buffered) once it isn't a tty, so without this the
            // child's own output could appear before the echoed line.
            std::cout << line << "\n" << std::flush;
            if (!runRecipeLine(line)) {
                recipeFailed = true;
                break;
            }
        }

        if (recipeFailed) {
            if (!keepGoing) failFlag = true;
            statuses[i] = BuildStatus::Failed;
        } else {
            statuses[i] = BuildStatus::Built;
        }
    }

    return statuses;
}

} // namespace bt
