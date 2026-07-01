#ifndef SA_RULES_SA004_MISSING_RETURN_H
#define SA_RULES_SA004_MISSING_RETURN_H

#include "rules/rule.h"

namespace sa {

// Flags non-`void` functions whose last statement doesn't guarantee a
// `return`: an exhaustive if/else (both branches always exit), or an
// infinite (`while (1)`/`do ... while (1)`) loop with no same-scope
// `break`, both count as always exiting; anything else (including a bare
// `if` with no `else`) does not.
class MissingReturn : public Rule {
public:
    static constexpr const char *kRuleId = "SA004";

    std::string id() const override { return kRuleId; }
    std::vector<Diagnostic> check(const TSTree *tree, const std::string &source, const std::string &path,
                                   const Config &config) const override;
};

} // namespace sa

#endif // SA_RULES_SA004_MISSING_RETURN_H
