#include "macro_expander.h"

#include <cassert>
#include <iostream>
#include <string>

#include "macro_table.h"
#include "pp_tokenizer.h"

namespace {

void define(pp::MacroTable &table, const std::string &name, const std::string &replacementText) {
    pp::MacroTable::MacroDef def;
    def.name = name;
    def.replacement = pp::PPTokenizer(replacementText).tokenize();
    def.definedInFile = "<test>";
    def.definedAtLine = 1;
    table.define(std::move(def));
}

void expectExpansion(const pp::MacroTable &table, const std::string &input,
                      const std::string &expected) {
    std::string actual = pp::expandText(input, table);
    if (actual != expected) {
        std::cerr << "expandText(\"" << input << "\")\n  expected: \"" << expected
                   << "\"\n  actual:   \"" << actual << "\"\n";
        std::abort();
    }
}

} // namespace

int main() {
    // Basic object-like substitution.
    {
        pp::MacroTable table;
        define(table, "MAX", "100");
        expectExpansion(table, "x = MAX;", "x = 100;");
    }

    // Empty-valued macro.
    {
        pp::MacroTable table;
        define(table, "FLAG", "");
        expectExpansion(table, "a FLAG b", "a  b");
    }

    // Nested expansion: a macro's replacement referencing another macro is rescanned.
    {
        pp::MacroTable table;
        define(table, "PI", "3");
        define(table, "TWO_PI", "(PI * 2)");
        expectExpansion(table, "c = TWO_PI;", "c = (3 * 2);");
    }

    // Self-reference: hide-set prevents infinite recursion.
    {
        pp::MacroTable table;
        define(table, "X", "X + 1");
        expectExpansion(table, "v = X;", "v = X + 1;");
    }

    // Mutual recursion: expanding from either side terminates on that side's name.
    {
        pp::MacroTable table;
        define(table, "A", "B");
        define(table, "B", "A");
        expectExpansion(table, "A", "A");
        expectExpansion(table, "B", "B");
    }

    // Three-way cycle: not special-cased to pairs.
    {
        pp::MacroTable table;
        define(table, "A", "B");
        define(table, "B", "C");
        define(table, "C", "A");
        expectExpansion(table, "A", "A");
    }

    // The hide set blocks only the originating macro, not siblings it expands to.
    {
        pp::MacroTable table;
        define(table, "A", "B");
        define(table, "B", "5");
        expectExpansion(table, "A", "5");
    }

    // Maximal munch at the tokenizer level: X does not match inside XY.
    {
        pp::MacroTable table;
        define(table, "X", "1");
        expectExpansion(table, "XY = 2;", "XY = 2;");
    }

    // Macros are never expanded inside string or character literals.
    {
        pp::MacroTable table;
        define(table, "VERSION", "2");
        expectExpansion(table, "\"VERSION 1.0\"", "\"VERSION 1.0\"");
        define(table, "A", "1");
        expectExpansion(table, "'A'", "'A'");
    }

    // No surrounding whitespace required.
    {
        pp::MacroTable table;
        define(table, "X", "1");
        expectExpansion(table, "(X)", "(1)");
    }

    // Undefined identifiers pass through unchanged.
    {
        pp::MacroTable table;
        expectExpansion(table, "totally_undefined + 1", "totally_undefined + 1");
    }

    // MacroTable: undefine, redefinition (last-wins).
    {
        pp::MacroTable table;
        define(table, "X", "1");
        assert(table.isDefined("X"));
        table.undefine("X");
        assert(!table.isDefined("X"));
        table.undefine("NEVER_DEFINED"); // no-op, must not throw
        expectExpansion(table, "X", "X");

        define(table, "Y", "1");
        define(table, "Y", "2");
        expectExpansion(table, "Y", "2");
    }

    std::cout << "macro_expander_test: all checks passed\n";
    return 0;
}
