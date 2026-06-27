#include "makefile.h"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <unistd.h>

namespace {

void expect(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::abort();
    }
}

void expectThrows(const std::string &text, const std::string &messageSubstring) {
    try {
        bt::parseMakefile(text);
    } catch (const bt::MakefileParseError &e) {
        std::string what = e.what();
        expect(what.find(messageSubstring) != std::string::npos,
               "expected error containing \"" + messageSubstring + "\", got \"" + what + "\"");
        return;
    }
    expect(false, "expected parseMakefile to throw for: " + text);
}

std::filesystem::path scratchDir(const std::string &tag) {
    std::filesystem::path dir = std::filesystem::temp_directory_path() /
                                 ("build-tool-parser-test-" + tag + "-" + std::to_string(getpid()));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

std::string lowercase(std::string s) {
    for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

} // namespace

int main() {
    // parses_a_simple_rule
    {
        bt::ParsedMakefile parsed = bt::parseMakefile("foo: bar baz\n\techo building foo\n");
        expect(parsed.rules.size() == 1, "expected 1 rule");
        const bt::Rule &rule = parsed.rules[0];
        expect(rule.target == "foo", "target should be foo");
        expect((rule.prereqs == std::vector<std::string>{"bar", "baz"}), "prereqs should be [bar, baz]");
        expect((rule.recipe == std::vector<std::string>{"echo building foo"}), "recipe mismatch");
        expect(!rule.phony, "should not be phony");
    }

    // parses_multiple_recipe_lines
    {
        bt::ParsedMakefile parsed = bt::parseMakefile("foo:\n\techo one\n\techo two\n");
        expect((parsed.rules[0].recipe == std::vector<std::string>{"echo one", "echo two"}),
               "recipe should have two lines");
    }

    // target_with_no_prereqs
    {
        bt::ParsedMakefile parsed = bt::parseMakefile("foo:\n\techo hi\n");
        expect(parsed.rules[0].prereqs.empty(), "prereqs should be empty");
    }

    // phony_declaration_marks_matching_rule
    {
        bt::ParsedMakefile parsed = bt::parseMakefile(".PHONY: clean\nclean:\n\trm -f out\n");
        expect(parsed.phonyTargets.count("clean") == 1, "clean should be a phony target");
        const bt::Rule *rule = parsed.ruleFor("clean");
        expect(rule != nullptr && rule->phony, "clean rule should be marked phony");
    }

    // phony_can_precede_or_follow_the_rule
    {
        bt::ParsedMakefile before = bt::parseMakefile(".PHONY: clean\nclean:\n\trm -f out\n");
        bt::ParsedMakefile after = bt::parseMakefile("clean:\n\trm -f out\n.PHONY: clean\n");
        expect(before.ruleFor("clean")->phony, "before: clean should be phony");
        expect(after.ruleFor("clean")->phony, "after: clean should be phony");
    }

    // comments_and_blank_lines_are_skipped
    {
        bt::ParsedMakefile parsed = bt::parseMakefile(
            "# this is a comment\n\nfoo: bar # inline comment\n\techo hi\n\n# trailing\n");
        expect(parsed.rules.size() == 1, "expected 1 rule");
        expect((parsed.rules[0].prereqs == std::vector<std::string>{"bar"}), "prereqs should be [bar]");
    }

    // default_goal_is_first_non_dot_target
    {
        bt::ParsedMakefile parsed =
            bt::parseMakefile(".PHONY: all\nall: a b\na:\n\techo a\nb:\n\techo b\n");
        std::optional<std::string> goal = parsed.defaultGoal();
        expect(goal.has_value() && *goal == "all", "default goal should be 'all'");
    }

    // recipe_line_without_a_preceding_rule_is_an_error
    expectThrows("\techo orphan\n", "1: recipe line has no target");

    // header_without_colon_is_an_error
    expectThrows("this has no colon\n", "1: missing separator ':'");

    // discover_finds_a_makefile
    {
        std::filesystem::path dir = scratchDir("found");
        std::ofstream(dir / "makefile") << "foo:\n\techo hi\n";

        std::optional<std::filesystem::path> found = bt::discoverMakefile(dir);
        expect(found.has_value(), "should find a makefile");
        expect(lowercase(found->filename().string()) == "makefile",
               "found filename should lowercase to 'makefile'");

        std::filesystem::remove_all(dir);
    }

    // discover_returns_none_when_absent
    {
        std::filesystem::path dir = scratchDir("absent");
        expect(!bt::discoverMakefile(dir).has_value(), "should not find a makefile");
        std::filesystem::remove_all(dir);
    }

    std::cout << "makefile_parser_test: all checks passed\n";
    return 0;
}
