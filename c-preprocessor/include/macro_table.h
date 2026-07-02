#ifndef PP_MACRO_TABLE_H
#define PP_MACRO_TABLE_H

#include <string>
#include <unordered_map>
#include <vector>

#include "token.h"

namespace pp {

// One shared MacroTable is threaded through an entire translation unit
// (including every recursively #include'd file), so #define is a global
// namespace. Macro bodies are stored as raw, unexpanded tokens captured at
// #define time and are only expanded lazily when a use is rescanned — this
// gives #undef late-binding semantics for free (a macro defined in terms of
// another macro that is later #undef'd will, once undef'd, expand its
// no-longer-defined reference literally).
class MacroTable {
public:
    struct MacroDef {
        std::string name;
        std::vector<PPToken> replacement;
        std::string definedInFile;
        int definedAtLine = 0;
    };

    // Overwrites any existing definition of the same name (last-wins, no
    // redefinition diagnostic — a deliberate simplification versus strict C).
    void define(MacroDef def);

    // No-op if `name` is not currently defined.
    void undefine(const std::string &name);

    const MacroDef *lookup(const std::string &name) const;
    bool isDefined(const std::string &name) const;

private:
    std::unordered_map<std::string, MacroDef> macros_;
};

} // namespace pp

#endif // PP_MACRO_TABLE_H
