#ifndef SA_RULES_SA005_UNREACHABLE_CODE_H
#define SA_RULES_SA005_UNREACHABLE_CODE_H

#include "rules/rule.h"

namespace sa {

// Flags the first statement following a `return`/`break`/`continue`/`goto`
// within the same block (compound statement or `case` body) — only the
// first such occurrence per block is reported.
class UnreachableCode : public Rule {
public:
    static constexpr const char *kRuleId = "SA005";

    std::string id() const override { return kRuleId; }
    std::vector<Diagnostic> check(const TSTree *tree, const std::string &source, const std::string &path,
                                   const Config &config) const override;
};

} // namespace sa

#endif // SA_RULES_SA005_UNREACHABLE_CODE_H
