#include "preprocessor.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "diagnostics.h"

namespace {

std::filesystem::path scratchDir() {
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "cpreproc_directive_test";
    std::filesystem::create_directories(dir);
    return dir;
}

std::filesystem::path writeTemp(const std::string &name, const std::string &content) {
    std::filesystem::path path = scratchDir() / name;
    std::ofstream out(path, std::ios::binary);
    assert(out && "could not create scratch fixture file");
    out << content;
    return path;
}

std::string preprocess(const std::filesystem::path &path) {
    pp::Preprocessor preprocessor;
    return preprocessor.preprocessFile(path.string());
}

void expectThrows(const std::filesystem::path &path, const std::string &messageSubstring) {
    try {
        preprocess(path);
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

} // namespace

int main() {
    // #define applies to every following line in the file.
    {
        auto path = writeTemp("basic_define.c", "#define MAX 100\nint x = MAX;\nint y = MAX + 1;\n");
        std::string out = preprocess(path);
        assert(out == "\nint x = 100;\nint y = 100 + 1;\n");
    }

    // #define with no value defines an empty macro.
    {
        auto path = writeTemp("empty_define.c", "#define FLAG\nint x = 1 FLAG;\n");
        std::string out = preprocess(path);
        assert(out == "\nint x = 1 ;\n");
    }

    // #undef removes a macro; a later use is literal.
    {
        auto path = writeTemp("undef.c", "#define X 1\n#undef X\nint y = X;\n");
        std::string out = preprocess(path);
        assert(out == "\n\nint y = X;\n");
    }

    // #undef of a never-defined name is a silent no-op.
    {
        auto path = writeTemp("undef_noop.c", "#undef NEVER_DEFINED\nint x = 1;\n");
        std::string out = preprocess(path);
        assert(out == "\nint x = 1;\n");
    }

    // Malformed #define.
    expectThrows(writeTemp("define_no_name.c", "#define\n"), "macro name");
    expectThrows(writeTemp("define_bad_name.c", "#define 2X 1\n"), "macro name");

    // Function-like macros are rejected with a clear error naming the macro.
    expectThrows(writeTemp("define_function_like.c", "#define SQUARE(x) ((x)*(x))\n"),
                 "function-like macros are not supported");

    // Every unsupported directive is a hard error naming the directive.
    expectThrows(writeTemp("d_ifdef.c", "#ifdef FOO\n"), "#ifdef");
    expectThrows(writeTemp("d_ifndef.c", "#ifndef FOO\n"), "#ifndef");
    expectThrows(writeTemp("d_if.c", "#if 1\n"), "#if");
    expectThrows(writeTemp("d_elif.c", "#elif 1\n"), "#elif");
    expectThrows(writeTemp("d_else.c", "#else\n"), "#else");
    expectThrows(writeTemp("d_endif.c", "#endif\n"), "#endif");
    expectThrows(writeTemp("d_pragma.c", "#pragma once\n"), "#pragma");
    expectThrows(writeTemp("d_error.c", "#error \"boom\"\n"), "#error");
    expectThrows(writeTemp("d_line.c", "#line 10\n"), "#line");
    expectThrows(writeTemp("d_unknown.c", "#frobnicate\n"), "#frobnicate");

    // Leading whitespace before '#' is still recognized as a directive.
    {
        auto path = writeTemp("indented_define.c", "   #define X 1\nint y = X;\n");
        std::string out = preprocess(path);
        assert(out == "\nint y = 1;\n");
    }

    // A bare '#' line is a silent no-op, not an error.
    {
        auto path = writeTemp("bare_hash.c", "#\nint x = 1;\n");
        std::string out = preprocess(path);
        assert(out == "\nint x = 1;\n");
    }

    // Trailing tokens after #undef's name are rejected.
    expectThrows(writeTemp("undef_extra.c", "#define FOO 1\n#undef FOO BAR\n"),
                 "unexpected tokens after #undef");

    std::cout << "directive_test: all checks passed\n";
    return 0;
}
