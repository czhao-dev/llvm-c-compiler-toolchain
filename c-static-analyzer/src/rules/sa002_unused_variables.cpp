#include "rules/sa002_unused_variables.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

#include "visitor.h"

namespace sa {

namespace {

// Drills through wrapper declarators (pointer/array/init) to the raw
// `identifier`, or a null TSNode if none is found.
TSNode baseIdentifier(TSNode declarator) {
    TSNode current = declarator;
    while (nodeKind(current) != "identifier") {
        TSNode next = childByField(current, "declarator");
        if (ts_node_is_null(next)) return TSNode{};
        current = next;
    }
    return current;
}

// First declaration site per local variable name, keyed by name.
std::unordered_map<std::string, TSNode> declaredNames(TSNode body, const std::string &source) {
    std::unordered_map<std::string, TSNode> declared;
    std::vector<TSNode> nodes;
    walk(body, nodes);
    for (TSNode node : nodes) {
        if (nodeKind(node) != "declaration") continue;
        for (TSNode declarator : childrenByField(node, "declarator")) {
            TSNode nameNode = baseIdentifier(declarator);
            if (ts_node_is_null(nameNode)) continue;
            std::string name = nodeText(nameNode, source);
            if (!name.empty() && name.front() == '_') continue;
            declared.emplace(name, nameNode); // emplace: first declaration site wins
        }
    }
    return declared;
}

bool isPlainAssignmentTarget(TSNode node, const std::string &source) {
    TSNode parent = ts_node_parent(node);
    if (ts_node_is_null(parent) || nodeKind(parent) != "assignment_expression") return false;
    TSNode left = childByField(parent, "left");
    TSNode op = childByField(parent, "operator");
    if (ts_node_is_null(left) || left.id != node.id) return false;
    return !ts_node_is_null(op) && nodeText(op, source) == "=";
}

std::unordered_set<std::string> collectUsed(TSNode body, const std::string &source,
                                             const std::unordered_set<const void *> &declaredSiteIds) {
    std::unordered_set<std::string> used;
    std::vector<TSNode> nodes;
    walk(body, nodes);
    for (TSNode node : nodes) {
        if (nodeKind(node) != "identifier") continue;
        if (declaredSiteIds.count(node.id) != 0 || isPlainAssignmentTarget(node, source)) continue;
        used.insert(nodeText(node, source));
    }
    return used;
}

} // namespace

std::vector<Diagnostic> UnusedVariables::check(const TSTree *tree, const std::string &source,
                                                const std::string &path, const Config & /*config*/) const {
    std::vector<Diagnostic> diagnostics;
    std::vector<TSNode> nodes;
    walk(ts_tree_root_node(tree), nodes);

    for (TSNode func : nodes) {
        if (nodeKind(func) != "function_definition") continue;
        TSNode body = childByField(func, "body");
        if (ts_node_is_null(body)) continue;

        std::unordered_map<std::string, TSNode> declared = declaredNames(body, source);
        std::unordered_set<const void *> declaredSiteIds;
        for (const auto &[name, node] : declared) declaredSiteIds.insert(node.id);
        std::unordered_set<std::string> used = collectUsed(body, source, declaredSiteIds);

        for (const auto &[name, nameNode] : declared) {
            if (used.count(name) != 0) continue;
            auto [line, col] = loc(nameNode);
            Diagnostic d;
            d.path = path;
            d.line = line;
            d.col = col;
            d.ruleId = kRuleId;
            d.message = "Local variable `" + name + "` is assigned but never used";
            diagnostics.push_back(std::move(d));
        }
    }

    std::sort(diagnostics.begin(), diagnostics.end());
    return diagnostics;
}

} // namespace sa
