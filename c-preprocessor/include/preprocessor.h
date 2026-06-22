#ifndef PP_PREPROCESSOR_H
#define PP_PREPROCESSOR_H

#include <filesystem>
#include <string>
#include <vector>

#include "macro_table.h"

namespace pp {

struct PreprocessorOptions {
    // -I directories, searched in the order given, after the directory of
    // the file containing the #include directive.
    std::vector<std::string> includeSearchPaths;
};

// Orchestrates the whole pipeline for a translation unit: comment-strips
// and line-scans a file, dispatching #include/#define/#undef and
// macro-expanding every other line, recursing into included files while
// threading a single shared MacroTable through the entire file tree.
class Preprocessor {
public:
    explicit Preprocessor(PreprocessorOptions options = {});

    // Entry point. Resolves inputPath, processes it (recursively splicing
    // #includes), and returns the fully preprocessed text.
    std::string preprocessFile(const std::string &inputPath);

private:
    void processFile(const std::filesystem::path &path, std::string &output);
    void processLine(const std::string &line, const std::filesystem::path &file, int lineNo,
                      std::string &output);
    void dispatchDirective(const std::string &directiveLine, const std::filesystem::path &file,
                            int lineNo, std::string &output);
    void handleInclude(const std::string &args, const std::filesystem::path &file, int lineNo,
                        std::string &output);
    void handleDefine(const std::string &args, const std::filesystem::path &file, int lineNo);
    void handleUndef(const std::string &args, const std::filesystem::path &file, int lineNo);
    std::filesystem::path resolveInclude(const std::string &filename,
                                          const std::filesystem::path &includingDir,
                                          const std::filesystem::path &file, int lineNo);

    MacroTable macros_;
    PreprocessorOptions options_;
    // Canonicalized absolute paths of the currently-active #include chain
    // (innermost last) — used only for circular-include detection, not
    // global include deduplication, so diamond includes are intentionally
    // processed more than once (there are no include guards).
    std::vector<std::filesystem::path> includeStack_;
};

} // namespace pp

#endif // PP_PREPROCESSOR_H
