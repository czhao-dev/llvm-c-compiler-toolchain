#include "makefile.h"

#include <cctype>
#include <sstream>
#include <system_error>

namespace bt {

namespace {

std::string stripComment(const std::string &line) {
    auto pos = line.find('#');
    return pos == std::string::npos ? line : line.substr(0, pos);
}

std::string trim(const std::string &s) {
    static const char *kWhitespace = " \t\r\n";
    auto start = s.find_first_not_of(kWhitespace);
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(kWhitespace);
    return s.substr(start, end - start + 1);
}

std::vector<std::string> splitWhitespace(const std::string &s) {
    std::vector<std::string> out;
    std::istringstream iss(s);
    std::string token;
    while (iss >> token) out.push_back(token);
    return out;
}

// Mirrors Rust's `str::lines()`: splits on '\n', additionally stripping a
// paired '\r' immediately before it (a lone '\r' not followed by '\n' is
// left untouched), and a trailing newline does not produce an extra empty
// final line.
std::vector<std::string> splitLines(const std::string &text) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start <= text.size()) {
        std::size_t newlinePos = text.find('\n', start);
        if (newlinePos == std::string::npos) {
            if (start < text.size()) lines.push_back(text.substr(start));
            break;
        }
        std::string line = text.substr(start, newlinePos - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(std::move(line));
        start = newlinePos + 1;
    }
    return lines;
}

} // namespace

std::optional<std::string> ParsedMakefile::defaultGoal() const {
    for (const Rule &rule : rules) {
        if (rule.target.empty() || rule.target.front() != '.') return rule.target;
    }
    return std::nullopt;
}

const Rule *ParsedMakefile::ruleFor(const std::string &target) const {
    for (const Rule &rule : rules) {
        if (rule.target == target) return &rule;
    }
    return nullptr;
}

ParsedMakefile parseMakefile(const std::string &text) {
    ParsedMakefile result;
    std::optional<std::size_t> current;

    std::vector<std::string> lines = splitLines(text);
    for (std::size_t idx = 0; idx < lines.size(); ++idx) {
        const std::string &rawLine = lines[idx];
        std::size_t lineNo = idx + 1;

        if (!rawLine.empty() && rawLine.front() == '\t') {
            if (!current.has_value()) {
                throw MakefileParseError(std::to_string(lineNo) + ": recipe line has no target");
            }
            result.rules[*current].recipe.push_back(rawLine.substr(1));
            continue;
        }

        std::string trimmed = trim(stripComment(rawLine));
        if (trimmed.empty()) continue;

        std::size_t colonIdx = trimmed.find(':');
        if (colonIdx == std::string::npos) {
            throw MakefileParseError(std::to_string(lineNo) + ": missing separator ':'");
        }

        std::string head = trim(trimmed.substr(0, colonIdx));
        std::string rest = trim(trimmed.substr(colonIdx + 1));
        std::vector<std::string> names = splitWhitespace(rest);

        if (head == ".PHONY") {
            for (std::string &name : names) result.phonyTargets.insert(std::move(name));
            current.reset();
            continue;
        }

        current = result.rules.size();
        Rule rule;
        rule.target = std::move(head);
        rule.prereqs = std::move(names);
        result.rules.push_back(std::move(rule));
    }

    for (Rule &rule : result.rules) {
        if (result.phonyTargets.count(rule.target) != 0) rule.phony = true;
    }

    return result;
}

std::optional<std::filesystem::path> discoverMakefile(const std::filesystem::path &dir) {
    for (const char *name : {"Makefile", "makefile"}) {
        std::filesystem::path candidate = dir / name;
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate, ec)) return candidate;
    }
    return std::nullopt;
}

} // namespace bt
