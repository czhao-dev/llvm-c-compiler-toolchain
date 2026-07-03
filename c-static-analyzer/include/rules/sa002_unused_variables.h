#ifndef SA_RULES_SA002_UNUSED_VARIABLES_H
#define SA_RULES_SA002_UNUSED_VARIABLES_H

#include "rules/rule.h"

namespace sa {

// Flags local variables (declared via a `declaration` node inside a
// function body, name not starting with `_`) that are never read — only
// ever assigned to via a plain `=`, or never referenced at all.
class UnusedVariables : public Rule {
public:
    static constexpr const char *kRuleId = "SA002";

    std::string id() const override { return kRuleId; }
    std::vector<Diagnostic> check(const TSTree *tree, const std::string &source, const std::string &path,
                                   const Config &config) const override;
};

} // namespace sa

#endif // SA_RULES_SA002_UNUSED_VARIABLES_H
