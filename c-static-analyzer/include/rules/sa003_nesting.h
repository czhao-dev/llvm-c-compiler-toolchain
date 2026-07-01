#ifndef SA_RULES_SA003_NESTING_H
#define SA_RULES_SA003_NESTING_H

#include "rules/rule.h"

namespace sa {

// Flags control flow (if/for/while/do/switch) nested deeper than
// config.maxNesting, reporting only the first violation per function.
// `else if` chains do not add a nesting level; a genuine `else { }` block
// does.
class Nesting : public Rule {
public:
    static constexpr const char *kRuleId = "SA003";

    std::string id() const override { return kRuleId; }
    std::vector<Diagnostic> check(const TSTree *tree, const std::string &source, const std::string &path,
                                   const Config &config) const override;
};

} // namespace sa

#endif // SA_RULES_SA003_NESTING_H
