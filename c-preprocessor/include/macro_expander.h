#ifndef PP_MACRO_EXPANDER_H
#define PP_MACRO_EXPANDER_H

#include <string>
#include <vector>

#include "macro_table.h"
#include "token.h"

namespace pp {

// Pure functions: no file I/O, no directive awareness. Fully expand macro
// references in `tokens`/`text` using `macros`, applying hide-set
// rescanning (see macro_expander.cpp) so self-referential and mutually
// recursive macros terminate deterministically instead of looping forever.
std::vector<PPToken> expandMacros(const std::vector<PPToken> &tokens, const MacroTable &macros);
std::string expandText(const std::string &text, const MacroTable &macros);

} // namespace pp

#endif // PP_MACRO_EXPANDER_H
