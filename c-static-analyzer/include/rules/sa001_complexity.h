#ifndef SA_RULES_SA001_COMPLEXITY_H
#define SA_RULES_SA001_COMPLEXITY_H

#include "rules/rule.h"

namespace sa {

// Cyclomatic complexity: 1, plus one for every branch (if/for/while/do/
// ternary), boolean short-circuit operator (&&, ||), and non-default case
// label, summed over a whole function body. Flags functions above
// config.maxComplexity.
class Complexity : public Rule {
public:
    static constexpr const char *kRuleId = "SA001";

    std::string id() const override { return kRuleId; }
    std::vector<Diagnostic> check(const TSTree *tree, const std::string &source, const std::string &path,
                                   const Config &config) const override;
};

} // namespace sa

#endif // SA_RULES_SA001_COMPLEXITY_H
