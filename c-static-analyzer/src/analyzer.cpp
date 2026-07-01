#include "analyzer.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>
#include <system_error>

#include <tree_sitter/api.h>
#include <tree_sitter/tree-sitter-c.h>

#include "fnmatch.h"
#include "rules/rule.h"
#include "rules/sa001_complexity.h"
#include "rules/sa002_unused_variables.h"
#include "rules/sa003_nesting.h"
#include "rules/sa004_missing_return.h"
#include "rules/sa005_unreachable_code.h"

namespace sa {

const std::vector<std::string> kDefaultExcludeDirs = {
    ".git", "build", "dist", "cmake-build-debug", "cmake-build-release", "CMakeFiles", "out", "vendor",
    "third_party",
};

namespace {

const std::vector<std::string> kExtensions = {"c", "h"};

bool hasCOrHExtension(const std::filesystem::path &path) {
    std::string ext = path.extension().string();
    if (!ext.empty() && ext.front() == '.') ext = ext.substr(1);
    return ext == "c" || ext == "h";
}

void collectCFiles(const std::filesystem::path &dir, std::vector<std::filesystem::path> &out) {
    std::error_code ec;
    std::filesystem::directory_iterator it(dir, ec);
    if (ec) return;
    for (const auto &entry : it) {
        const std::filesystem::path &path = entry.path();
        std::error_code isDirEc;
        if (std::filesystem::is_directory(path, isDirEc)) {
            collectCFiles(path, out);
        } else if (hasCOrHExtension(path)) {
            out.push_back(path);
        }
    }
}

const Complexity kComplexityRule;
const UnusedVariables kUnusedVariablesRule;
const Nesting kNestingRule;
const MissingReturn kMissingReturnRule;
const UnreachableCode kUnreachableCodeRule;

const Rule *const kAllRules[] = {
    &kComplexityRule, &kUnusedVariablesRule, &kNestingRule, &kMissingReturnRule, &kUnreachableCodeRule,
};

} // namespace

bool isExcluded(const std::filesystem::path &path, const std::vector<std::string> &patterns) {
    for (const auto &component : path) {
        std::string comp = component.string();
        for (const auto &excludedDir : kDefaultExcludeDirs) {
            if (comp == excludedDir) return true;
        }
    }

    std::string posix = path.string();
    std::replace(posix.begin(), posix.end(), '\\', '/');
    std::string filename = path.filename().string();

    for (const auto &pattern : patterns) {
        if (fnmatch(posix, pattern) || fnmatch(filename, pattern)) return true;
    }
    return false;
}

std::vector<std::filesystem::path> iterCFiles(const std::vector<std::filesystem::path> &paths,
                                               const std::vector<std::string> &exclude) {
    std::vector<std::filesystem::path> out;
    for (const auto &path : paths) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(path, ec)) {
            if (hasCOrHExtension(path) && !isExcluded(path, exclude)) out.push_back(path);
            continue;
        }

        std::vector<std::filesystem::path> found;
        collectCFiles(path, found);
        std::sort(found.begin(), found.end());
        for (const auto &candidate : found) {
            if (!isExcluded(candidate, exclude)) out.push_back(candidate);
        }
    }
    return out;
}

std::vector<Diagnostic> analyzeFile(const std::filesystem::path &path, const Config &config) {
    std::string pathStr = path.string();

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        Diagnostic d;
        d.path = pathStr;
        d.line = 1;
        d.col = 0;
        d.ruleId = "SA000";
        d.message = std::string("Could not read file: ") + std::strerror(errno);
        return {d};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    std::string source = ss.str();

    TSParser *parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_c());
    TSTree *tree = ts_parser_parse_string(parser, nullptr, source.c_str(), static_cast<uint32_t>(source.size()));

    std::vector<Diagnostic> diagnostics;
    for (const Rule *rule : kAllRules) {
        if (!config.isEnabled(rule->id())) continue;
        std::vector<Diagnostic> ruleDiagnostics = rule->check(tree, source, pathStr, config);
        diagnostics.insert(diagnostics.end(), ruleDiagnostics.begin(), ruleDiagnostics.end());
    }

    ts_tree_delete(tree);
    ts_parser_delete(parser);
    return diagnostics;
}

std::vector<Diagnostic> analyzePaths(const std::vector<std::filesystem::path> &paths, const Config &config) {
    std::vector<Diagnostic> diagnostics;
    for (const auto &filePath : iterCFiles(paths, config.exclude)) {
        std::vector<Diagnostic> fileDiagnostics = analyzeFile(filePath, config);
        diagnostics.insert(diagnostics.end(), fileDiagnostics.begin(), fileDiagnostics.end());
    }
    std::sort(diagnostics.begin(), diagnostics.end());
    return diagnostics;
}

} // namespace sa
