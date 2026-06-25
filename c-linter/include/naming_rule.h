#ifndef CL_NAMING_RULE_H
#define CL_NAMING_RULE_H

#include "diagnostic.h"
#include "token.h"

#include <string>
#include <vector>

namespace cl {

// Flags every Identifier token that contains an uppercase letter and no
// underscore (e.g. totalCount, TotalCount) — the literal snake_case check.
// MAX_SIZE and total_count are not flagged since they contain an
// underscore; single-word ALLCAPS with no underscore (e.g. PI) is flagged,
// matching the rule as specified.
std::vector<Diagnostic> checkNamingRule(const std::vector<Token> &tokens,
                                         const std::string &filename);

} // namespace cl

#endif // CL_NAMING_RULE_H
