#include "analyzer.h"

#include <algorithm>
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
                                 ("sa-analyzer-test-" + tag + "-" + std::to_string(getpid()));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

void writeFile(const std::filesystem::path &path, const std::string &contents) {
    std::ofstream out(path, std::ios::binary);
    out << contents;
}

std::vector<std::string> fileNames(const std::vector<std::filesystem::path> &paths) {
    std::vector<std::string> names;
    for (const auto &p : paths) names.push_back(p.filename().string());
    std::sort(names.begin(), names.end());
    return names;
}

} // namespace

int main() {
    // default_excludes_skip_build_and_vcs_dirs
    {
        std::filesystem::path dir = scratchDir("excludes");
        std::filesystem::create_directories(dir / "src");
        writeFile(dir / "src" / "app.c", "int main(void) { return 0; }\n");
        std::filesystem::create_directories(dir / "build" / "obj");
        writeFile(dir / "build" / "obj" / "generated.c", "int x;\n");

        sa::Config config;
        std::vector<std::string> found = fileNames(sa::iterCFiles({dir}, config.exclude));
        expect((found == std::vector<std::string>{"app.c"}), "only app.c should be found");

        std::filesystem::remove_all(dir);
    }

    // custom_exclude_pattern
    {
        std::filesystem::path dir = scratchDir("custom-exclude");
        writeFile(dir / "keep.c", "int x;\n");
        writeFile(dir / "generated.c", "int x;\n");

        std::vector<std::string> found = fileNames(sa::iterCFiles({dir}, {"*generated*"}));
        expect((found == std::vector<std::string>{"keep.c"}), "only keep.c should be found");

        std::filesystem::remove_all(dir);
    }

    // discovers_header_files
    {
        std::filesystem::path dir = scratchDir("headers");
        writeFile(dir / "lib.h", "int add(int a, int b);\n");
        writeFile(dir / "lib.c", "int add(int a, int b) { return a + b; }\n");

        std::vector<std::string> found = fileNames(sa::iterCFiles({dir}, {}));
        expect((found == std::vector<std::string>{"lib.c", "lib.h"}), "both lib.c and lib.h should be found");

        std::filesystem::remove_all(dir);
    }

    // excludes_default_dirs (mirrors the analyzer.rs inline unit test)
    {
        std::filesystem::path path = "project/build/obj/foo.c";
        expect(sa::isExcluded(path, {}), "a path under build/ should be excluded by default");
    }

    // excludes_user_glob_patterns (fnmatch is a full anchored match)
    {
        std::filesystem::path path = "tests/test_foo.c";
        expect(sa::isExcluded(path, {"tests/*"}), "tests/* should exclude tests/test_foo.c");
        expect(!sa::isExcluded(path, {"other/*"}), "other/* should not exclude tests/test_foo.c");
        expect(sa::isExcluded(path, {"test_*.c"}), "test_*.c should exclude tests/test_foo.c via the filename");
    }

    std::cout << "analyzer_test: all checks passed\n";
    return 0;
}
