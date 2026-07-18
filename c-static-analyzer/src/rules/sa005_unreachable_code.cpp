#include "rules/sa005_unreachable_code.h"

#include "cfg.h"
#include "visitor.h"

namespace sa {

namespace {

const char *keywordFor(const std::string &kind) {
    if (kind == "return_statement") return "return";
    if (kind == "break_statement") return "break";
    if (kind == "continue_statement") return "continue";
    if (kind == "goto_statement") return "goto";
    return nullptr;
}

std::vector<TSNode> caseBodyStmts(TSNode caseStmt) {
    TSNode value = childByField(caseStmt, "value");
    std::vector<TSNode> result;
    for (TSNode child : namedChildren(caseStmt)) {
        if (!ts_node_is_null(value) && child.id == value.id) continue;
        result.push_back(child);
    }
    return result;
}

// The keyword that directly precedes `stmt` in its enclosing block/case, if
// any -- used only to render a more specific "Unreachable code after
// `return`"-style message when the cause is a simple explicit jump.
// `stmt` being unreachable at all is determined by CFG reachability, not
// by this adjacency check.
const char *causeKeyword(TSNode stmt) {
    TSNode parent = ts_node_parent(stmt);
    if (ts_node_is_null(parent)) return nullptr;

    std::vector<TSNode> siblings;
    std::string parentKind = nodeKind(parent);
    if (parentKind == "compound_statement") {
        siblings = namedChildren(parent);
    } else if (parentKind == "case_statement") {
        siblings = caseBodyStmts(parent);
    } else {
        return nullptr;
    }

    for (std::size_t i = 0; i < siblings.size(); ++i) {
        if (siblings[i].id != stmt.id) continue;
        if (i == 0) return nullptr;
        return keywordFor(nodeKind(siblings[i - 1]));
    }
    return nullptr;
}

} // namespace

std::vector<Diagnostic> UnreachableCode::check(const TSTree *tree, const std::string &source,
                                                const std::string &path, const Config & /*config*/) const {
    std::vector<Diagnostic> diagnostics;
    std::vector<TSNode> nodes;
    walk(ts_tree_root_node(tree), nodes);

    for (TSNode func : nodes) {
        if (nodeKind(func) != "function_definition") continue;
        TSNode body = childByField(func, "body");
        if (ts_node_is_null(body)) continue;

        CFG cfg = buildCFG(body, source);
        const std::vector<bool> &reachable = cfg.reachableFromEntry();

        for (std::size_t id = 0; id < cfg.blockCount(); ++id) {
            if (reachable[id]) continue;
            const BasicBlock &block = cfg.block(id);
            if (block.statements.empty()) continue;

            TSNode firstStmt = block.statements.front();
            auto [line, col] = loc(firstStmt);
            const char *keyword = causeKeyword(firstStmt);

            Diagnostic d;
            d.path = path;
            d.line = line;
            d.col = col;
            d.ruleId = kRuleId;
            d.message = keyword != nullptr ? std::string("Unreachable code after `") + keyword + "`"
                                            : "Unreachable code";
            diagnostics.push_back(std::move(d));
        }
    }
    return diagnostics;
}

} // namespace sa
