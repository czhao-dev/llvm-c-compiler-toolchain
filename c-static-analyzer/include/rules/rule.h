#ifndef SA_RULES_RULE_H
#define SA_RULES_RULE_H

#include <string>
#include <vector>

#include <tree_sitter/api.h>

#include "config.h"
#include "diagnostic.h"

namespace sa {

class Rule {
public:
    virtual ~Rule() = default;
    virtual std::string id() const = 0;
    virtual std::vector<Diagnostic> check(const TSTree *tree, const std::string &source,
                                           const std::string &path, const Config &config) const = 0;
};

} // namespace sa

#endif // SA_RULES_RULE_H
