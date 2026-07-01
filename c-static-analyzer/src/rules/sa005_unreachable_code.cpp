#include "rules/sa005_unreachable_code.h"

#include <optional>

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

std::optional<Diagnostic> checkBlock(const std::vector<TSNode> &stmts, const std::string &path) {
    for (std::size_t i = 0; i + 1 < stmts.size(); ++i) {
        const char *keyword = keywordFor(nodeKind(stmts[i]));
        if (keyword == nullptr) continue;

        auto [line, col] = loc(stmts[i + 1]);
        Diagnostic d;
        d.path = path;
        d.line = line;
        d.col = col;
        d.ruleId = UnreachableCode::kRuleId;
        d.message = std::string("Unreachable code after `") + keyword + "`";
        return d;
    }
    return std::nullopt;
}

} // namespace

std::vector<Diagnostic> UnreachableCode::check(const TSTree *tree, const std::string & /*source*/,
                                                const std::string &path, const Config & /*config*/) const {
    std::vector<Diagnostic> diagnostics;
    std::vector<TSNode> nodes;
    walk(ts_tree_root_node(tree), nodes);

    for (TSNode node : nodes) {
        std::string kind = nodeKind(node);
        std::optional<Diagnostic> diagnostic;
        if (kind == "compound_statement") {
            diagnostic = checkBlock(namedChildren(node), path);
        } else if (kind == "case_statement") {
            diagnostic = checkBlock(caseBodyStmts(node), path);
        }
        if (diagnostic.has_value()) diagnostics.push_back(std::move(*diagnostic));
    }
    return diagnostics;
}

} // namespace sa
