#include "config.h"

#include <cctype>
#include <fstream>
#include <optional>
#include <sstream>
#include <system_error>

namespace sa {

namespace {

// Just enough of TOML to express this tool's own config schema: flat
// `key = "string"`, `key = 123`, and `key = ["a", "b", ...]` assignments,
// `#` comments, and blank lines. Not a general-purpose TOML parser.
struct RawConfig {
    std::optional<std::vector<std::string>> exclude;
    std::optional<std::int64_t> maxComplexity;
    std::optional<std::int64_t> maxNesting;
    std::optional<std::vector<std::string>> enabledRules;
};

std::string trim(const std::string &s) {
    static const char *kWhitespace = " \t\r\n";
    auto start = s.find_first_not_of(kWhitespace);
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(kWhitespace);
    return s.substr(start, end - start + 1);
}

std::optional<std::vector<std::string>> parseStringArray(const std::string &value) {
    std::string trimmed = trim(value);
    if (trimmed.size() < 2 || trimmed.front() != '[' || trimmed.back() != ']') return std::nullopt;

    std::string inner = trimmed.substr(1, trimmed.size() - 2);
    std::vector<std::string> items;
    std::size_t i = 0;
    while (i < inner.size()) {
        while (i < inner.size() && (std::isspace(static_cast<unsigned char>(inner[i])) || inner[i] == ',')) {
            ++i;
        }
        if (i >= inner.size()) break;
        if (inner[i] != '"') return std::nullopt; // only quoted strings are supported

        ++i;
        std::string item;
        while (i < inner.size() && inner[i] != '"') {
            item += inner[i];
            ++i;
        }
        if (i >= inner.size()) return std::nullopt; // unterminated string
        ++i; // skip closing quote
        items.push_back(std::move(item));
    }
    return items;
}

std::optional<std::int64_t> parseInteger(const std::string &value) {
    std::string trimmed = trim(value);
    if (trimmed.empty()) return std::nullopt;
    try {
        std::size_t pos = 0;
        long long parsed = std::stoll(trimmed, &pos);
        if (pos != trimmed.size()) return std::nullopt;
        return static_cast<std::int64_t>(parsed);
    } catch (const std::exception &) {
        return std::nullopt;
    }
}

// Strips a `#` comment, but only outside of a double-quoted string.
std::string stripComment(const std::string &line) {
    std::string result;
    bool inQuotes = false;
    for (char c : line) {
        if (c == '"') inQuotes = !inQuotes;
        if (c == '#' && !inQuotes) break;
        result += c;
    }
    return result;
}

std::optional<RawConfig> parseConfigToml(const std::string &text) {
    RawConfig raw;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        std::string trimmed = trim(stripComment(line));
        if (trimmed.empty()) continue;

        std::size_t eq = trimmed.find('=');
        if (eq == std::string::npos) return std::nullopt;
        std::string key = trim(trimmed.substr(0, eq));
        std::string value = trim(trimmed.substr(eq + 1));

        if (key == "exclude") {
            auto parsed = parseStringArray(value);
            if (!parsed.has_value()) return std::nullopt;
            raw.exclude = std::move(*parsed);
        } else if (key == "max_complexity") {
            auto parsed = parseInteger(value);
            if (!parsed.has_value()) return std::nullopt;
            raw.maxComplexity = *parsed;
        } else if (key == "max_nesting") {
            auto parsed = parseInteger(value);
            if (!parsed.has_value()) return std::nullopt;
            raw.maxNesting = *parsed;
        } else if (key == "enabled_rules") {
            auto parsed = parseStringArray(value);
            if (!parsed.has_value()) return std::nullopt;
            raw.enabledRules = std::move(*parsed);
        }
        // Unrecognized keys are ignored, matching serde's default
        // (non-`deny_unknown_fields`) behavior.
    }
    return raw;
}

} // namespace

bool Config::isEnabled(const std::string &ruleId) const {
    if (enabledRules.empty()) return true;
    for (const std::string &rule : enabledRules) {
        if (rule == ruleId) return true;
    }
    return false;
}

Config loadConfig(const std::filesystem::path &start) {
    std::filesystem::path dir = start;
    while (true) {
        std::filesystem::path candidate = dir / kConfigFilename;
        std::error_code ec;
        if (std::filesystem::is_regular_file(candidate, ec)) {
            std::ifstream in(candidate, std::ios::binary);
            if (!in) return Config{};

            std::ostringstream ss;
            ss << in.rdbuf();
            std::optional<RawConfig> raw = parseConfigToml(ss.str());
            if (!raw.has_value()) return Config{};

            Config config;
            if (raw->exclude.has_value()) config.exclude = *raw->exclude;
            if (raw->maxComplexity.has_value()) config.maxComplexity = *raw->maxComplexity;
            if (raw->maxNesting.has_value()) config.maxNesting = *raw->maxNesting;
            if (raw->enabledRules.has_value()) config.enabledRules = *raw->enabledRules;
            return config;
        }

        std::filesystem::path parent = dir.parent_path();
        if (parent == dir) break; // reached the root
        dir = parent;
    }
    return Config{};
}

} // namespace sa
