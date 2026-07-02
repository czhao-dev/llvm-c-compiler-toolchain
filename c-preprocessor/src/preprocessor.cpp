#include "preprocessor.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "comment_stripper.h"
#include "diagnostics.h"
#include "macro_expander.h"
#include "pp_tokenizer.h"

namespace pp {

namespace {

std::string readFile(const std::filesystem::path &path, const std::string &referencedFrom) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw PreprocessorError(referencedFrom, 0, "cannot open file \"" + path.string() + "\"");
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

std::string trimLeft(const std::string &s) {
    std::size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r')) i += 1;
    return s.substr(i);
}

std::string trimRight(const std::string &s) {
    std::size_t end = s.size();
    while (end > 0 && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r')) end -= 1;
    return s.substr(0, end);
}

std::string trim(const std::string &s) { return trimRight(trimLeft(s)); }

// Splits on '\n' without producing a trailing empty entry for a final
// newline, so the number of lines matches std::count(text, '\n') exactly
// (or that count + 1 when the file doesn't end in a newline).
std::vector<std::string> splitLines(const std::string &text) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= text.size(); ++i) {
        if (i == text.size() || text[i] == '\n') {
            lines.push_back(text.substr(start, i - start));
            start = i + 1;
        }
    }
    if (!text.empty() && text.back() == '\n' && !lines.empty()) {
        lines.pop_back();
    }
    return lines;
}

} // namespace

Preprocessor::Preprocessor(PreprocessorOptions options) : options_(std::move(options)) {}

std::string Preprocessor::preprocessFile(const std::string &inputPath) {
    std::filesystem::path path(inputPath);
    if (!std::filesystem::exists(path)) {
        throw PreprocessorError(inputPath, 0, "cannot open file \"" + inputPath + "\"");
    }

    includeStack_.push_back(std::filesystem::weakly_canonical(path));
    std::string output;
    processFile(path, output);
    includeStack_.pop_back();
    return output;
}

void Preprocessor::processFile(const std::filesystem::path &path, std::string &output) {
    std::string raw = readFile(path, path.string());
    std::string stripped = stripComments(raw, path.string());
    std::vector<std::string> lines = splitLines(stripped);

    for (std::size_t idx = 0; idx < lines.size(); ++idx) {
        processLine(lines[idx], path, static_cast<int>(idx) + 1, output);
    }
}

void Preprocessor::processLine(const std::string &line, const std::filesystem::path &file,
                                int lineNo, std::string &output) {
    std::string leftTrimmed = trimLeft(line);
    if (!leftTrimmed.empty() && leftTrimmed[0] == '#') {
        dispatchDirective(leftTrimmed, file, lineNo, output);
        return;
    }
    output += expandText(line, macros_);
    output += '\n';
}

void Preprocessor::dispatchDirective(const std::string &directiveLine,
                                      const std::filesystem::path &file, int lineNo,
                                      std::string &output) {
    std::string rest = trimLeft(directiveLine.substr(1)); // drop leading '#'
    if (rest.empty()) {
        output += '\n'; // a bare '#' line is a silent no-op, but still occupies a line
        return;
    }

    std::size_t nameEnd = 0;
    while (nameEnd < rest.size() && isIdentifierContinue(rest[nameEnd])) nameEnd += 1;
    std::string name = rest.substr(0, nameEnd);
    std::string args = trim(rest.substr(nameEnd));

    // #include splices the included file's own lines into `output`, so it
    // must not add an extra blank line of its own. #define/#undef consume
    // no lines of their own, so (like a real preprocessor) they leave a
    // blank line behind to keep subsequent line numbers lined up with the
    // original source.
    if (name == "include") {
        handleInclude(args, file, lineNo, output);
    } else if (name == "define") {
        handleDefine(args, file, lineNo);
        output += '\n';
    } else if (name == "undef") {
        handleUndef(args, file, lineNo);
        output += '\n';
    } else {
        throw PreprocessorError(file.string(), lineNo,
                                 "unsupported preprocessor directive '#" + name + "'");
    }
}

void Preprocessor::handleInclude(const std::string &args, const std::filesystem::path &file,
                                  int lineNo, std::string &output) {
    if (!args.empty() && args.front() == '<') {
        throw PreprocessorError(file.string(), lineNo,
                                 "angle-bracket #include <...> is not supported; use \"...\"");
    }
    if (args.size() < 2 || args.front() != '"') {
        throw PreprocessorError(file.string(), lineNo, "malformed #include directive");
    }

    std::size_t closeQuote = args.find('"', 1);
    if (closeQuote == std::string::npos) {
        throw PreprocessorError(file.string(), lineNo, "unterminated #include filename");
    }
    std::string filename = args.substr(1, closeQuote - 1);
    if (filename.empty()) {
        throw PreprocessorError(file.string(), lineNo, "empty #include filename");
    }
    std::string trailing = trim(args.substr(closeQuote + 1));
    if (!trailing.empty()) {
        throw PreprocessorError(file.string(), lineNo,
                                 "unexpected tokens after #include \"" + filename + "\"");
    }

    std::filesystem::path includingDir = file.has_parent_path() ? file.parent_path() : ".";
    std::filesystem::path resolved = resolveInclude(filename, includingDir, file, lineNo);
    std::filesystem::path canonical = std::filesystem::weakly_canonical(resolved);

    auto cycleStart = std::find(includeStack_.begin(), includeStack_.end(), canonical);
    if (cycleStart != includeStack_.end()) {
        std::string chain;
        for (auto it = cycleStart; it != includeStack_.end(); ++it) {
            chain += it->filename().string() + " -> ";
        }
        chain += canonical.filename().string();
        throw PreprocessorError(file.string(), lineNo, "circular #include detected: " + chain);
    }

    includeStack_.push_back(canonical);
    processFile(resolved, output);
    includeStack_.pop_back();
}

std::filesystem::path Preprocessor::resolveInclude(const std::string &filename,
                                                     const std::filesystem::path &includingDir,
                                                     const std::filesystem::path &file,
                                                     int lineNo) {
    std::filesystem::path candidate = includingDir / filename;
    if (std::filesystem::exists(candidate)) return candidate;

    std::string searched = candidate.string();
    for (const std::string &dir : options_.includeSearchPaths) {
        std::filesystem::path c = std::filesystem::path(dir) / filename;
        if (std::filesystem::exists(c)) return c;
        searched += ", " + c.string();
    }

    throw PreprocessorError(file.string(), lineNo,
                             "cannot find include file \"" + filename + "\" (searched: " +
                                 searched + ")");
}

void Preprocessor::handleDefine(const std::string &args, const std::filesystem::path &file,
                                 int lineNo) {
    if (args.empty() || !isIdentifierStart(args[0])) {
        throw PreprocessorError(file.string(), lineNo, "macro name must be an identifier");
    }

    std::size_t nameEnd = 0;
    while (nameEnd < args.size() && isIdentifierContinue(args[nameEnd])) nameEnd += 1;
    std::string name = args.substr(0, nameEnd);

    if (nameEnd < args.size() && args[nameEnd] == '(') {
        throw PreprocessorError(file.string(), lineNo,
                                 "function-like macros are not supported: '" + name + "'");
    }

    std::string replacementText = trimLeft(args.substr(nameEnd));

    MacroTable::MacroDef def;
    def.name = name;
    def.replacement = PPTokenizer(replacementText).tokenize();
    def.definedInFile = file.string();
    def.definedAtLine = lineNo;
    macros_.define(std::move(def));
}

void Preprocessor::handleUndef(const std::string &args, const std::filesystem::path &file,
                                int lineNo) {
    if (args.empty() || !isIdentifierStart(args[0])) {
        throw PreprocessorError(file.string(), lineNo, "macro name must be an identifier");
    }

    std::size_t nameEnd = 0;
    while (nameEnd < args.size() && isIdentifierContinue(args[nameEnd])) nameEnd += 1;
    std::string name = args.substr(0, nameEnd);

    std::string trailing = trim(args.substr(nameEnd));
    if (!trailing.empty()) {
        throw PreprocessorError(file.string(), lineNo, "unexpected tokens after #undef " + name);
    }
    macros_.undefine(name);
}

} // namespace pp
