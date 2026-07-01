#include "rules/sa001_complexity.h"

#include "visitor.h"

namespace sa {

namespace {

bool isBranchType(const std::string &kind) {
    return kind == "if_statement" || kind == "for_statement" || kind == "while_statement" ||
           kind == "do_statement" || kind == "conditional_expression";
}

bool isBoolOperator(const std::string &text) { return text == "&&" || text == "||"; }

long long score(TSNode node, const std::string &source) {
    long long total = 0;
    std::string kind = nodeKind(node);
    if (isBranchType(kind)) {
        total += 1;
    } else if (kind == "case_statement" && !ts_node_is_null(childByField(node, "value"))) {
        total += 1;
    } else if (kind == "binary_expression") {
        TSNode op = childByField(node, "operator");
        if (!ts_node_is_null(op) && isBoolOperator(nodeText(op, source))) total += 1;
    }
    for (TSNode child : children(node)) total += score(child, source);
    return total;
}

long long computeComplexity(TSNode func, const std::string &source) {
    long long complexity = 1;
    for (TSNode child : children(func)) complexity += score(child, source);
    return complexity;
}

} // namespace

std::vector<Diagnostic> Complexity::check(const TSTree *tree, const std::string &source,
                                           const std::string &path, const Config &config) const {
    std::vector<Diagnostic> diagnostics;
    long long threshold = config.maxComplexity;

    std::vector<TSNode> nodes;
    walk(ts_tree_root_node(tree), nodes);
    for (TSNode node : nodes) {
        if (nodeKind(node) != "function_definition") continue;

        long long complexityScore = computeComplexity(node, source);
        if (complexityScore > threshold) {
            auto [line, col] = loc(node);
            Diagnostic d;
            d.path = path;
            d.line = line;
            d.col = col;
            d.ruleId = kRuleId;
            d.message = "Function `" + functionName(node, source) + "` has cyclomatic complexity " +
                        std::to_string(complexityScore) + " (threshold " + std::to_string(threshold) + ")";
            diagnostics.push_back(std::move(d));
        }
    }
    return diagnostics;
}

} // namespace sa
