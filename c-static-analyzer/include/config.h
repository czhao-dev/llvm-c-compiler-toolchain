#ifndef SA_CONFIG_H
#define SA_CONFIG_H

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace sa {

inline constexpr const char *kConfigFilename = ".c-static-analyzer.toml";

struct Config {
    std::vector<std::string> exclude;
    std::int64_t maxComplexity = 10;
    std::int64_t maxNesting = 4;
    std::vector<std::string> enabledRules; // empty means "all enabled"

    bool isEnabled(const std::string &ruleId) const;
};

// Loads the nearest `.c-static-analyzer.toml`, searching `start` and then
// each ancestor directory in turn. The first one found wins (even if it
// fails to read or parse, in which case Config{} defaults are returned
// rather than continuing to search further up). Only the TOML fields
// present in the file override Config{}'s defaults.
Config loadConfig(const std::filesystem::path &start);

} // namespace sa

#endif // SA_CONFIG_H
