#include "rules/sa004_missing_return.h"

#include "cfg.h"
#include "visitor.h"

namespace sa {

namespace {

bool isVoidFunction(TSNode func, const std::string &source) {
    TSNode typeNode = childByField(func, "type");
    if (ts_node_is_null(typeNode) || nodeText(typeNode, source) != "void") return false;

    TSNode declarator = childByField(func, "declarator");
    while (!ts_node_is_null(declarator)) {
        std::string kind = nodeKind(declarator);
        if (kind == "function_declarator") return true;
        if (kind == "pointer_declarator") return false; // returns void*, a real value
        declarator = childByField(declarator, "declarator");
    }
    return false;
}

// Whether the function can fall off the end of its body without an
// explicit `return`: true iff `exit()` has an incoming Fallthrough edge
// (as opposed to a `return`'s Unconditional edge) whose source block is
// itself reachable from entry.
bool canFallOffEnd(const CFG &cfg) {
    const std::vector<bool> &reachable = cfg.reachableFromEntry();
    for (std::size_t id = 0; id < cfg.blockCount(); ++id) {
        if (!reachable[id]) continue;
        for (const CFGEdge &edge : cfg.block(id).successors) {
            if (edge.target == cfg.exit() && edge.kind == EdgeKind::Fallthrough) return true;
        }
    }
    return false;
}

} // namespace

std::vector<Diagnostic> MissingReturn::check(const TSTree *tree, const std::string &source,
                                              const std::string &path, const Config & /*config*/) const {
    std::vector<Diagnostic> diagnostics;
    std::vector<TSNode> nodes;
    walk(ts_tree_root_node(tree), nodes);

    for (TSNode func : nodes) {
        if (nodeKind(func) != "function_definition") continue;
        if (isVoidFunction(func, source)) continue;

        TSNode body = childByField(func, "body");
        if (ts_node_is_null(body)) continue;

        CFG cfg = buildCFG(body, source);
        if (canFallOffEnd(cfg)) {
            auto [line, col] = loc(func);
            Diagnostic d;
            d.path = path;
            d.line = line;
            d.col = col;
            d.ruleId = kRuleId;
            d.message =
                "Function `" + functionName(func, source) + "` may not return a value on all code paths";
            diagnostics.push_back(std::move(d));
        }
    }
    return diagnostics;
}

} // namespace sa
