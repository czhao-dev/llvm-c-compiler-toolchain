#include "cli.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_map>

#include "analyzer.h"
#include "diagnostic.h"
#include "snippet.h"

namespace sa {

namespace {

const char *kUsage =
    "usage: c-static-analyzer scan [paths...] [--max-complexity N] [--max-nesting N]\n"
    "                              [--select SA001,SA002] [--exclude PATTERN]...\n"
    "                              [--no-config] [--show-source] [-h|--help]\n";

// Splits on '\n' without keeping the newline; a trailing partial line (no
// final '\n') is still included, matching how diagnostics reference it.
std::vector<std::string> splitLines(const std::string &contents) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start <= contents.size()) {
        std::size_t end = contents.find('\n', start);
        if (end == std::string::npos) {
            lines.push_back(contents.substr(start));
            break;
        }
        lines.push_back(contents.substr(start, end - start));
        start = end + 1;
    }
    return lines;
}

std::string trim(const std::string &s) {
    static const char *kWhitespace = " \t\r\n";
    auto start = s.find_first_not_of(kWhitespace);
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(kWhitespace);
    return s.substr(start, end - start + 1);
}

std::int64_t parseInt64(const std::string &value, const std::string &flagName) {
    try {
        std::size_t pos = 0;
        long long parsed = std::stoll(value, &pos);
        if (pos != value.size()) throw std::invalid_argument("trailing characters");
        return static_cast<std::int64_t>(parsed);
    } catch (const std::exception &) {
        throw CliError("invalid value for " + flagName + ": '" + value + "'");
    }
}

} // namespace

ScanArgs parseArgs(const std::vector<std::string> &args) {
    ScanArgs result;

    if (!args.empty() && (args[0] == "-h" || args[0] == "--help")) {
        result.showHelp = true;
        return result;
    }
    if (args.empty() || args[0] != "scan") {
        throw CliError("expected 'scan' subcommand");
    }

    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string &arg = args[i];
        if (arg == "-h" || arg == "--help") {
            result.showHelp = true;
        } else if (arg == "--max-complexity") {
            if (i + 1 >= args.size()) throw CliError("--max-complexity requires a value");
            result.maxComplexity = parseInt64(args[++i], "--max-complexity");
        } else if (arg == "--max-nesting") {
            if (i + 1 >= args.size()) throw CliError("--max-nesting requires a value");
            result.maxNesting = parseInt64(args[++i], "--max-nesting");
        } else if (arg == "--select") {
            if (i + 1 >= args.size()) throw CliError("--select requires a value");
            result.select = args[++i];
        } else if (arg == "--exclude") {
            if (i + 1 >= args.size()) throw CliError("--exclude requires a value");
            result.exclude.push_back(args[++i]);
        } else if (arg == "--no-config") {
            result.noConfig = true;
        } else if (arg == "--show-source") {
            result.showSource = true;
        } else if (!arg.empty() && arg.front() == '-') {
            throw CliError("unknown flag '" + arg + "'");
        } else {
            result.paths.push_back(arg);
        }
    }

    return result;
}

Config buildConfig(const ScanArgs &args) {
    Config config = args.noConfig ? Config{} : loadConfig(std::filesystem::current_path());

    if (args.maxComplexity.has_value()) config.maxComplexity = *args.maxComplexity;
    if (args.maxNesting.has_value()) config.maxNesting = *args.maxNesting;
    if (args.select.has_value()) {
        config.enabledRules.clear();
        std::stringstream ss(*args.select);
        std::string token;
        while (std::getline(ss, token, ',')) {
            std::string trimmed = trim(token);
            if (!trimmed.empty()) config.enabledRules.push_back(trimmed);
        }
    }
    for (const auto &pattern : args.exclude) config.exclude.push_back(pattern);

    return config;
}

int runScan(const ScanArgs &args) {
    std::vector<std::string> rawPaths = args.paths.empty() ? std::vector<std::string>{"."} : args.paths;
    std::vector<std::filesystem::path> paths(rawPaths.begin(), rawPaths.end());

    for (const auto &path : paths) {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            std::cerr << "error: path not found: " << path.string() << "\n";
            return 2;
        }
    }

    Config config = buildConfig(args);
    std::vector<Diagnostic> diagnostics = analyzePaths(paths, config);

    // Diagnostics are a flat, globally-sorted list decoupled from any open
    // file, so --show-source needs its own on-demand per-path line cache
    // rather than reusing anything from the scan itself.
    std::unordered_map<std::string, std::vector<std::string>> lineCache;
    for (const auto &diagnostic : diagnostics) {
        if (!args.showSource) {
            std::cout << toString(diagnostic) << "\n";
            continue;
        }

        auto it = lineCache.find(diagnostic.path);
        if (it == lineCache.end()) {
            std::ifstream in(diagnostic.path, std::ios::binary);
            std::ostringstream ss;
            ss << in.rdbuf();
            it = lineCache.emplace(diagnostic.path, splitLines(ss.str())).first;
        }
        const std::vector<std::string> &lines = it->second;

        if (diagnostic.line >= 1 && diagnostic.line <= lines.size()) {
            std::cout << formatWithSource(diagnostic, lines[diagnostic.line - 1]) << "\n";
        } else {
            std::cout << toString(diagnostic) << "\n";
        }
    }

    if (diagnostics.empty()) return 0;
    std::cerr << "\n" << diagnostics.size() << " issue(s) found.\n";
    return 1;
}

int run(int argc, char **argv) {
    std::vector<std::string> args(argv + 1, argv + argc);

    ScanArgs parsed;
    try {
        parsed = parseArgs(args);
    } catch (const std::exception &e) {
        std::cerr << "error: " << e.what() << "\n" << kUsage;
        return 2;
    }

    if (parsed.showHelp) {
        std::cout << kUsage;
        return 0;
    }

    return runScan(parsed);
}

} // namespace sa
