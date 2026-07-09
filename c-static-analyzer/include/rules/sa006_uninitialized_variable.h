#ifndef SA_RULES_SA006_UNINITIALIZED_VARIABLE_H
#define SA_RULES_SA006_UNINITIALIZED_VARIABLE_H

#include "rules/rule.h"

namespace sa {

// Flags a local variable (declared via a `declaration` node inside a
// function body, no initializer, name not starting with `_`) whose first
// subsequent reference is a read rather than a write. Array-typed locals
// are not analyzed (see docs/SPEC.md). This is a textual, single-pass
// check, not control-flow-sensitive dataflow analysis.
class UninitializedVariable : public Rule {
public:
    static constexpr const char *kRuleId = "SA006";

    std::string id() const override { return kRuleId; }
    std::vector<Diagnostic> check(const TSTree *tree, const std::string &source, const std::string &path,
                                   const Config &config) const override;
};

} // namespace sa

#endif // SA_RULES_SA006_UNINITIALIZED_VARIABLE_H
