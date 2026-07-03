#include "config.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
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

std::filesystem::path scratchDir(const std::string &tag) {
    std::filesystem::path dir = std::filesystem::temp_directory_path() /
                                 ("sa-config-test-" + tag + "-" + std::to_string(getpid()));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

} // namespace

int main() {
    // default_config_enables_all_rules
    {
        sa::Config config;
        expect(config.isEnabled("SA001"), "default config should enable SA001");
        expect(config.isEnabled("SA005"), "default config should enable SA005");
    }

    // enabled_rules_restricts_selection
    {
        sa::Config config;
        config.enabledRules = {"SA001"};
        expect(config.isEnabled("SA001"), "SA001 should be enabled");
        expect(!config.isEnabled("SA002"), "SA002 should not be enabled");
    }

    // load_config_reads_dedicated_file
    {
        std::filesystem::path dir = scratchDir("dedicated");
        std::ofstream(dir / sa::kConfigFilename) << "max_complexity = 5\nexclude = [\"vendor/*\"]\n";

        sa::Config config = sa::loadConfig(dir);
        expect(config.maxComplexity == 5, "max_complexity should be 5");
        expect((config.exclude == std::vector<std::string>{"vendor/*"}), "exclude should be [vendor/*]");
        expect(config.maxNesting == 4, "max_nesting should keep its default of 4");

        std::filesystem::remove_all(dir);
    }

    // load_config_falls_back_to_default_when_missing
    {
        std::filesystem::path dir = scratchDir("missing");
        sa::Config config = sa::loadConfig(dir);
        expect(config.maxComplexity == 10, "max_complexity should default to 10");
        std::filesystem::remove_all(dir);
    }

    std::cout << "config_test: all checks passed\n";
    return 0;
}
