// Exercises the built build-tool binary end-to-end via subprocess
// invocations, matching the original tool's build_tool_e2e test suite.

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include <sys/wait.h>
#include <unistd.h>

#ifndef BT_BINARY
#define BT_BINARY "build-tool"
#endif

namespace {

void expect(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::abort();
    }
}

std::string shellQuote(const std::string &s) {
    std::string quoted = "'";
    for (char c : s) {
        if (c == '\'') {
            quoted += "'\"'\"'";
        } else {
            quoted += c;
        }
    }
    quoted += "'";
    return quoted;
}

int runBuildTool(const std::filesystem::path &dir) {
    std::string cmd = "cd " + shellQuote(dir.string()) + " && " + shellQuote(BT_BINARY) +
                       " >/dev/null 2>/dev/null";
    int status = std::system(cmd.c_str());
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

std::filesystem::path scratchDir(const std::string &tag) {
    std::filesystem::path dir = std::filesystem::temp_directory_path() /
                                 ("build-tool-e2e-" + tag + "-" + std::to_string(getpid()));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

void writeFile(const std::filesystem::path &path, const std::string &contents) {
    std::ofstream out(path, std::ios::binary);
    out << contents;
}

std::string readFile(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

int countLines(const std::string &s) {
    return static_cast<int>(std::count(s.begin(), s.end(), '\n'));
}

} // namespace

int main() {
    // diamond_build_skips_recipes_when_up_to_date_then_rebuilds_after_touch
    {
        std::filesystem::path dir = scratchDir("diamond");
        writeFile(dir / "common.h", "shared header");
        writeFile(dir / "Makefile",
                  "all: a.out b.out\n"
                  "a.out: common.h\n"
                  "\techo a >> build.log\n"
                  "\ttouch a.out\n"
                  "b.out: common.h\n"
                  "\techo b >> build.log\n"
                  "\ttouch b.out\n");

        expect(runBuildTool(dir) == 0, "first run should succeed");
        expect(std::filesystem::exists(dir / "a.out"), "a.out should exist");
        expect(std::filesystem::exists(dir / "b.out"), "b.out should exist");
        expect(countLines(readFile(dir / "build.log")) == 2, "both recipes should run once");

        expect(runBuildTool(dir) == 0, "second run should succeed");
        expect(countLines(readFile(dir / "build.log")) == 2,
               "an unchanged rebuild should not rerun recipes");

        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        writeFile(dir / "common.h", "shared header v2");

        expect(runBuildTool(dir) == 0, "third run should succeed");
        expect(countLines(readFile(dir / "build.log")) == 4,
               "touching the shared prerequisite should rebuild both targets");

        std::filesystem::remove_all(dir);
    }

    // failing_recipe_exits_with_failure_status
    {
        std::filesystem::path dir = scratchDir("failure");
        writeFile(dir / "Makefile", "all:\n\tfalse\n");
        expect(runBuildTool(dir) == 1, "a failing recipe should exit 1");
        std::filesystem::remove_all(dir);
    }

    // missing_makefile_exits_with_usage_error
    {
        std::filesystem::path dir = scratchDir("missing");
        expect(runBuildTool(dir) == 2, "a missing Makefile should exit 2");
        std::filesystem::remove_all(dir);
    }

    std::cout << "build_tool_e2e_test: all checks passed\n";
    return 0;
}
