#include "planner.h"
#include "makefile.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

namespace {

void expect(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::abort();
    }
}

std::filesystem::path scratchDir(const std::string &tag) {
    std::filesystem::path dir = std::filesystem::temp_directory_path() /
                                 ("build-tool-planner-" + tag + "-" + std::to_string(getpid()));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

bt::Rule makeRule(std::string target, std::vector<std::string> prereqs,
                   std::vector<std::string> recipe, bool phony) {
    bt::Rule rule;
    rule.target = std::move(target);
    rule.prereqs = std::move(prereqs);
    rule.recipe = std::move(recipe);
    rule.phony = phony;
    return rule;
}

bt::ParsedMakefile makeParsed(std::vector<bt::Rule> rules) {
    bt::ParsedMakefile parsed;
    parsed.rules = std::move(rules);
    return parsed;
}

void writeFile(const std::filesystem::path &path, const std::string &contents) {
    std::ofstream out(path, std::ios::binary);
    out << contents;
}

} // namespace

int main() {
    // diamond_dependency_builds_shared_node_once
    {
        std::filesystem::path dir = scratchDir("diamond");
        std::filesystem::path shared = dir / "shared.out";
        std::filesystem::path counter = dir / "counter.txt";
        std::filesystem::path a = dir / "a.out";
        std::filesystem::path b = dir / "b.out";

        std::vector<bt::Rule> rules;
        rules.push_back(makeRule(shared.string(), {},
                                  {"echo run >> " + counter.string(), "touch " + shared.string()},
                                  false));
        rules.push_back(makeRule(a.string(), {shared.string()}, {"touch " + a.string()}, false));
        rules.push_back(makeRule(b.string(), {shared.string()}, {"touch " + b.string()}, false));

        std::vector<std::string> goals{a.string(), b.string()};
        bt::Plan builtPlan = bt::plan(makeParsed(rules), goals);
        std::vector<bt::BuildStatus> statuses = bt::execute(builtPlan, false);

        for (std::size_t idx : builtPlan.goalIndices) {
            expect(statuses[idx] == bt::BuildStatus::Built, "diamond goal should be Built");
        }

        std::ifstream in(counter);
        std::string line;
        int count = 0;
        while (std::getline(in, line)) ++count;
        expect(count == 1, "a dependency shared by two parents should only build once");

        std::filesystem::remove_all(dir);
    }

    // cycle_is_detected
    {
        std::vector<bt::Rule> rules{makeRule("a", {"b"}, {}, false), makeRule("b", {"a"}, {}, false)};
        bool threw = false;
        try {
            bt::plan(makeParsed(rules), {"a"});
        } catch (const bt::PlanError &e) {
            threw = true;
            std::string what = e.what();
            expect(what.find('a') != std::string::npos && what.find('b') != std::string::npos,
                   "cycle message should mention both a and b");
        }
        expect(threw, "expected a cycle error");
    }

    // missing_rule_and_file_is_an_error
    {
        std::vector<bt::Rule> rules{makeRule("a", {"does-not-exist-anywhere"}, {}, false)};
        bool threw = false;
        try {
            bt::plan(makeParsed(rules), {"a"});
        } catch (const bt::PlanError &e) {
            threw = true;
            std::string what = e.what();
            expect(what.find("does-not-exist-anywhere") != std::string::npos,
                   "message should name the missing target");
            expect(what.find("'a'") != std::string::npos, "message should name the needed-by target");
        }
        expect(threw, "expected a NoRule error");
    }

    // no_rule_but_existing_file_is_a_leaf
    {
        std::filesystem::path dir = scratchDir("leaf");
        std::filesystem::path source = dir / "source.txt";
        writeFile(source, "hi");
        std::filesystem::path target = dir / "target.out";

        std::vector<bt::Rule> rules{
            makeRule(target.string(), {source.string()}, {"touch " + target.string()}, false)};
        bt::Plan builtPlan = bt::plan(makeParsed(rules), {target.string()});
        std::vector<bt::BuildStatus> statuses = bt::execute(builtPlan, false);
        expect(statuses[builtPlan.goalIndices[0]] == bt::BuildStatus::Built,
               "leaf-backed target should build");

        std::filesystem::remove_all(dir);
    }

    // target_newer_than_prereq_is_up_to_date
    {
        std::filesystem::path dir = scratchDir("uptodate");
        std::filesystem::path prereq = dir / "prereq.txt";
        std::filesystem::path target = dir / "target.out";
        std::filesystem::path marker = dir / "marker.txt";

        writeFile(prereq, "p");
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        writeFile(target, "t");

        std::vector<bt::Rule> rules{
            makeRule(target.string(), {prereq.string()}, {"touch " + marker.string()}, false)};
        bt::Plan builtPlan = bt::plan(makeParsed(rules), {target.string()});
        std::vector<bt::BuildStatus> statuses = bt::execute(builtPlan, false);
        expect(statuses[builtPlan.goalIndices[0]] == bt::BuildStatus::UpToDate,
               "target should be up to date");
        expect(!std::filesystem::exists(marker), "recipe should not have run");

        std::filesystem::remove_all(dir);
    }

    // phony_target_rebuilds_even_when_newer_than_prereqs
    {
        std::filesystem::path dir = scratchDir("phony");
        std::filesystem::path prereq = dir / "prereq.txt";
        std::filesystem::path target = dir / "target.out";

        writeFile(prereq, "p");
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        writeFile(target, "t");

        std::vector<bt::Rule> rules{makeRule(target.string(), {prereq.string()}, {"true"}, true)};
        bt::Plan builtPlan = bt::plan(makeParsed(rules), {target.string()});
        std::vector<bt::BuildStatus> statuses = bt::execute(builtPlan, false);
        expect(statuses[builtPlan.goalIndices[0]] == bt::BuildStatus::Built,
               "phony target should rebuild");

        std::filesystem::remove_all(dir);
    }

    // empty_recipe_phony_target_succeeds_trivially
    {
        std::vector<bt::Rule> rules{makeRule("noop", {}, {}, true)};
        bt::Plan builtPlan = bt::plan(makeParsed(rules), {"noop"});
        std::vector<bt::BuildStatus> statuses = bt::execute(builtPlan, false);
        expect(statuses[builtPlan.goalIndices[0]] == bt::BuildStatus::Built,
               "empty-recipe phony target should succeed");
    }

    // failed_prerequisite_causes_dependent_to_be_skipped
    for (bool keepGoing : {false, true}) {
        std::vector<bt::Rule> rules{
            makeRule("bad", {}, {"false"}, false),
            makeRule("downstream", {"bad"}, {"true"}, false),
        };
        bt::Plan builtPlan = bt::plan(makeParsed(rules), {"bad", "downstream"});
        std::vector<bt::BuildStatus> statuses = bt::execute(builtPlan, keepGoing);
        expect(statuses[builtPlan.goalIndices[0]] == bt::BuildStatus::Failed, "bad should fail");
        expect(statuses[builtPlan.goalIndices[1]] == bt::BuildStatus::Skipped,
               "downstream should be skipped");
    }

    std::cout << "planner_test: all checks passed\n";
    return 0;
}
