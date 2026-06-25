#ifndef CL_MAGIC_NUMBER_RULE_H
#define CL_MAGIC_NUMBER_RULE_H

#include "diagnostic.h"
#include "token.h"

#include <string>
#include <vector>

namespace cl {

// Flags a comparison operator (==, !=, <, >, <=, >=) immediately followed
// by an IntLiteral token (optionally through a unary minus, e.g. `!= -5`),
// suggesting a named macro instead. 0, 1, and -1 are exempt as common
// sentinel values (!= 0, >= 1, != -1) that aren't meaningfully "magic".
std::vector<Diagnostic> checkMagicNumberRule(const std::vector<Token> &tokens,
                                              const std::string &filename);

} // namespace cl

#endif // CL_MAGIC_NUMBER_RULE_H
