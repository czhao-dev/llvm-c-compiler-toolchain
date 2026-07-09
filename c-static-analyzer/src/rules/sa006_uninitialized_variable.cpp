#include "rules/sa006_uninitialized_variable.h"

#include <algorithm>
#include <unordered_map>

#include "visitor.h"

namespace sa {

namespace {

struct DeclaratorInfo {
    TSNode nameNode{};
    bool isArray = false;
};

// Drills through wrapper declarators (pointer/array) to the raw
// `identifier`, tracking whether an `array_declarator` was seen along the
// way. Returns a null `nameNode` if none is found.
DeclaratorInfo baseIdentifierInfo(TSNode declarator) {
    TSNode current = declarator;
    bool isArray = false;
    while (nodeKind(current) != "identifier") {
        if (nodeKind(current) == "array_declarator") isArray = true;
        TSNode next = childByField(current, "declarator");
        if (ts_node_is_null(next)) return DeclaratorInfo{TSNode{}, isArray};
        current = next;
    }
    return DeclaratorInfo{current, isArray};
}

// Local variables declared without an initializer, keyed by name (first
// declaration site wins, matching UnusedVariables's flat per-function
// tracking). Array-typed and underscore-prefixed locals are excluded.
std::unordered_map<std::string, TSNode> uninitializedDeclared(TSNode body, const std::string &source) {
    std::unordered_map<std::string, TSNode> declared;
    std::vector<TSNode> nodes;
    walk(body, nodes);
    for (TSNode node : nodes) {
        if (nodeKind(node) != "declaration") continue;
        for (TSNode declarator : childrenByField(node, "declarator")) {
            if (nodeKind(declarator) == "init_declarator") continue; // has an initializer
            DeclaratorInfo info = baseIdentifierInfo(declarator);
            if (ts_node_is_null(info.nameNode) || info.isArray) continue;
            std::string name = nodeText(info.nameNode, source);
            if (!name.empty() && name.front() == '_') continue;
            declared.emplace(name, info.nameNode);
        }
    }
    return declared;
}

// A reference counts as a write if it's the plain (`=`) left-hand side of
// an assignment, or the struct/pointer operand of a `.`/`->` field access
// that is itself the plain left-hand side of an assignment (covers the
// declare-then-initialize-field-by-field idiom, e.g. `pt.x = 1;`).
bool isWriteReference(TSNode node, const std::string &source) {
    TSNode parent = ts_node_parent(node);
    if (ts_node_is_null(parent)) return false;

    if (nodeKind(parent) == "assignment_expression") {
        TSNode left = childByField(parent, "left");
        TSNode op = childByField(parent, "operator");
        return !ts_node_is_null(left) && left.id == node.id && !ts_node_is_null(op) &&
               nodeText(op, source) == "=";
    }

    if (nodeKind(parent) == "field_expression") {
        TSNode argument = childByField(parent, "argument");
        if (ts_node_is_null(argument) || argument.id != node.id) return false;
        TSNode grandparent = ts_node_parent(parent);
        if (ts_node_is_null(grandparent) || nodeKind(grandparent) != "assignment_expression") return false;
        TSNode left = childByField(grandparent, "left");
        TSNode op = childByField(grandparent, "operator");
        return !ts_node_is_null(left) && left.id == parent.id && !ts_node_is_null(op) &&
               nodeText(op, source) == "=";
    }

    return false;
}

} // namespace

std::vector<Diagnostic> UninitializedVariable::check(const TSTree *tree, const std::string &source,
                                                       const std::string &path, const Config & /*config*/) const {
    std::vector<Diagnostic> diagnostics;
    std::vector<TSNode> topLevel;
    walk(ts_tree_root_node(tree), topLevel);

    for (TSNode func : topLevel) {
        if (nodeKind(func) != "function_definition") continue;
        TSNode body = childByField(func, "body");
        if (ts_node_is_null(body)) continue;

        std::unordered_map<std::string, TSNode> declared = uninitializedDeclared(body, source);
        if (declared.empty()) continue;

        std::vector<TSNode> bodyNodes;
        walk(body, bodyNodes);

        for (const auto &[name, declNode] : declared) {
            bool pastDecl = false;
            bool foundRef = false;
            TSNode refNode{};
            for (TSNode node : bodyNodes) {
                if (!pastDecl) {
                    if (node.id == declNode.id) pastDecl = true;
                    continue;
                }
                if (nodeKind(node) != "identifier" || nodeText(node, source) != name) continue;
                refNode = node;
                foundRef = true;
                break;
            }
            if (!foundRef || isWriteReference(refNode, source)) continue;

            auto [line, col] = loc(refNode);
            Diagnostic d;
            d.path = path;
            d.line = line;
            d.col = col;
            d.ruleId = kRuleId;
            d.message = "Local variable `" + name + "` may be used before being initialized";
            diagnostics.push_back(std::move(d));
        }
    }

    std::sort(diagnostics.begin(), diagnostics.end());
    return diagnostics;
}

} // namespace sa
