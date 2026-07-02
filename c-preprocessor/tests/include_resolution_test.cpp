#include "preprocessor.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "diagnostics.h"

#ifndef PP_FIXTURES_DIR
#define PP_FIXTURES_DIR "tests/fixtures"
#endif

namespace {

std::string fixture(const std::string &name) { return std::string(PP_FIXTURES_DIR) + "/" + name; }

std::string preprocess(const std::string &path,
                        const std::vector<std::string> &includeDirs = {}) {
    pp::PreprocessorOptions options;
    options.includeSearchPaths = includeDirs;
    pp::Preprocessor preprocessor(options);
    return preprocessor.preprocessFile(path);
}

void expectThrows(const std::string &path, const std::string &messageSubstring,
                   const std::vector<std::string> &includeDirs = {}) {
    try {
        preprocess(path, includeDirs);
    } catch (const pp::PreprocessorError &ex) {
        std::string what = ex.what();
        if (what.find(messageSubstring) == std::string::npos) {
            std::cerr << "processing " << path << "\n  expected error containing \""
                      << messageSubstring << "\", got \"" << what << "\"\n";
            std::abort();
        }
        return;
    }
    std::cerr << "expected processing " << path << " to throw\n";
    std::abort();
}

long occurrences(const std::string &haystack, const std::string &needle) {
    long count = 0;
    std::size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string::npos) {
        count += 1;
        pos += needle.size();
    }
    return count;
}

} // namespace

int main() {
    // Single-level include: header content spliced, and a macro defined in
    // the header is usable afterward (proves the single shared MacroTable).
    {
        std::string out = preprocess(fixture("main_uses_header_macro.c"));
        assert(out.find("int v = 42;") != std::string::npos);
    }

    // Resolution is relative to the INCLUDING file's directory, not the
    // top-level file's directory: rel_dir/rel_header.h's "rel_inner.h"
    // must resolve to rel_dir/rel_inner.h (REL_INNER_OK), not the decoy
    // tests/fixtures/rel_inner.h (WRONG_REL_INNER).
    {
        std::string out = preprocess(fixture("rel_main.c"));
        assert(out.find("REL_INNER_OK") != std::string::npos);
        assert(out.find("WRONG_REL_INNER") == std::string::npos);
    }

    // -I search path: unresolvable without it, resolvable with it.
    expectThrows(fixture("needs_extra_include.c"), "cannot find include file");
    {
        std::string out =
            preprocess(fixture("needs_extra_include.c"), {fixture("extra_include")});
        assert(out.find("flag = 1;") != std::string::npos);
    }

    // Precedence: a header found relative to the including file wins over
    // the same-named header found via -I.
    {
        std::string out =
            preprocess(fixture("precedence_main.c"), {fixture("precedence_extra")});
        assert(out.find("LOCAL_VERSION") != std::string::npos);
        assert(out.find("EXTRA_VERSION") == std::string::npos);
    }

    // Circular include: the chain is named in the error.
    expectThrows(fixture("circular_a.h"), "circular");
    {
        try {
            preprocess(fixture("circular_a.h"));
            std::abort();
        } catch (const pp::PreprocessorError &ex) {
            std::string what = ex.what();
            assert(what.find("circular_a.h") != std::string::npos);
            assert(what.find("circular_b.h") != std::string::npos);
        }
    }

    // A header that includes itself directly hits the same error path.
    expectThrows(fixture("self_include.h"), "circular");

    // Diamond include: NOT deduplicated — shared content appears twice
    // because there are no include guards.
    {
        std::string out = preprocess(fixture("diamond_top.c"));
        assert(occurrences(out, "DIAMOND_MARKER") == 2);
    }

    // Angle-bracket includes are explicitly unsupported.
    expectThrows(fixture("include_angle.c"), "angle-bracket");

    // Malformed #include variants. An unterminated quote is actually caught
    // one stage earlier, by the comment stripper's string-literal handling
    // (it runs over the whole file before any directive is parsed), so
    // handleInclude's own "unterminated #include filename" check exists
    // only as defensive-but-unreachable code for that case.
    expectThrows(fixture("include_no_quotes.c"), "malformed #include");
    expectThrows(fixture("include_unterminated_quote.c"), "unterminated string/character literal");
    expectThrows(fixture("include_trailing_tokens.c"), "unexpected tokens after #include");

    // Nonexistent include file names every directory searched.
    expectThrows(fixture("include_missing.c"), "cannot find include file");

    std::cout << "include_resolution_test: all checks passed\n";
    return 0;
}
