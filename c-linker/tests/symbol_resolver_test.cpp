#include "symbol_resolver.h"

#include <cstdlib>
#include <iostream>

#include "elf_reader.h"
#include "support/compile_fixture.h"

namespace {

void expect(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::abort();
    }
}

clnk::ObjectFile mustRead(const std::filesystem::path &path) {
    clnk::ReadResult result = clnk::readElfObject(path);
    expect(static_cast<bool>(result), path.string() + ": " + result.error);
    return std::move(*result.file);
}

} // namespace

int main() {
    auto scratch = makeScratchDir("symbol_resolver_test");

    // Case 1: a single definition resolves cleanly.
    {
        std::vector<clnk::ObjectFile> files;
        files.push_back(mustRead(compileNamedFixture("math", scratch)));

        std::vector<clnk::Diagnostic> diagnostics;
        clnk::SymbolTable table = clnk::buildSymbolTable(files, diagnostics);
        expect(diagnostics.empty(), "a single definition of `add` should not produce diagnostics");
        expect(table.definitions.count("add") == 1, "`add` should be in the symbol table");
    }

    // Case 2: two global definitions of the same name is an error.
    {
        std::vector<clnk::ObjectFile> files;
        files.push_back(mustRead(compileNamedFixture("math", scratch)));
        files.push_back(mustRead(compileNamedFixture("conflict_add", scratch)));

        std::vector<clnk::Diagnostic> diagnostics;
        clnk::buildSymbolTable(files, diagnostics);
        expect(diagnostics.size() == 1, "two definitions of `add` should produce exactly one diagnostic");
        expect(diagnostics[0].code == clnk::DiagnosticCode::MultipleDefinition, "diagnostic should be MultipleDefinition");
        expect(diagnostics[0].symbol == "add", "diagnostic should name `add`");
    }

    // Case 3: a reference with no definition anywhere is undefined.
    {
        std::vector<clnk::ObjectFile> files;
        files.push_back(mustRead(compileNamedFixture("start_calls_add", scratch)));

        std::vector<clnk::Diagnostic> diagnostics;
        clnk::SymbolTable table = clnk::buildSymbolTable(files, diagnostics);
        expect(diagnostics.empty(), "building the symbol table alone should not flag undefined symbols");

        bool undefinedOk = clnk::checkUndefinedSymbols(files, table, diagnostics);
        expect(!undefinedOk, "referencing `add` with no definition anywhere should fail");
        expect(diagnostics.size() == 1, "exactly one undefined-symbol diagnostic should be produced");
        expect(diagnostics[0].code == clnk::DiagnosticCode::UndefinedSymbol, "diagnostic should be UndefinedSymbol");
        expect(diagnostics[0].symbol == "add", "diagnostic should name `add`");
    }

    // Case 4: two files each defining a same-named `static` (local) symbol
    // never conflict -- local symbols are file-private.
    {
        std::vector<clnk::ObjectFile> files;
        files.push_back(mustRead(compileNamedFixture("local_helper_a", scratch)));
        files.push_back(mustRead(compileNamedFixture("local_helper_b", scratch)));

        std::vector<clnk::Diagnostic> diagnostics;
        clnk::SymbolTable table = clnk::buildSymbolTable(files, diagnostics);
        expect(diagnostics.empty(), "two files each defining a static `helper` should not conflict");
        expect(table.definitions.count("helper") == 0, "local symbols must never enter the global symbol table");
        expect(table.definitions.count("useHelperA") == 1, "useHelperA should be in the symbol table");
        expect(table.definitions.count("useHelperB") == 1, "useHelperB should be in the symbol table");
    }

    std::cout << "symbol_resolver_test: all checks passed\n";
    return 0;
}
