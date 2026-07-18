#ifndef SA_RULES_SA006_UNINITIALIZED_VARIABLE_H
#define SA_RULES_SA006_UNINITIALIZED_VARIABLE_H

#include "rules/rule.h"

namespace sa {

// Flags a local variable (declared via a `declaration` node inside a
// function body, no initializer, name not starting with `_`) that may be
// read before being written on some path through the function's CFG (a
// "may be uninitialized" forward dataflow, the same shape a real
// compiler's uninitialized-variable warning uses). Array-typed locals are
// not analyzed, and a write is only recognized as a plain `=` assignment
// (directly, or through a `.`/`->` field access) -- see docs/SPEC.md.
class UninitializedVariable : public Rule {
public:
    static constexpr const char *kRuleId = "SA006";

    std::string id() const override { return kRuleId; }
    std::vector<Diagnostic> check(const TSTree *tree, const std::string &source, const std::string &path,
                                   const Config &config) const override;
};

} // namespace sa

#endif // SA_RULES_SA006_UNINITIALIZED_VARIABLE_H
