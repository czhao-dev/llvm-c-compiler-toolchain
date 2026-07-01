#ifndef SA_ANALYZER_H
#define SA_ANALYZER_H

#include <filesystem>
#include <string>
#include <vector>

#include "config.h"
#include "diagnostic.h"

namespace sa {

extern const std::vector<std::string> kDefaultExcludeDirs;

// True if `path` should be excluded: either one of its path components
// exactly matches a default-excluded directory name, or one of `patterns`
// (glob, matched against both the posix-normalized full path and the bare
// filename) matches.
bool isExcluded(const std::filesystem::path &path, const std::vector<std::string> &patterns);

// Resolves `paths` (files scanned directly; directories walked
// recursively, without applying `exclude` during the walk itself — only
// as a post-filter) into a list of .c/.h files. Each directory argument's
// matches are sorted before filtering; the list is not re-sorted across
// multiple top-level path arguments.
std::vector<std::filesystem::path> iterCFiles(const std::vector<std::filesystem::path> &paths,
                                               const std::vector<std::string> &exclude);

// Parses and runs every enabled rule against a single file. An unreadable
// file yields a single synthetic "SA000" diagnostic instead of throwing.
std::vector<Diagnostic> analyzeFile(const std::filesystem::path &path, const Config &config);

// analyzeFile() over every file iterCFiles() finds, sorted globally by
// (path, line, col, ruleId, message).
std::vector<Diagnostic> analyzePaths(const std::vector<std::filesystem::path> &paths, const Config &config);

} // namespace sa

#endif // SA_ANALYZER_H
