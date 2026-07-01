#include "rules/sa003_nesting.h"

#include "visitor.h"

namespace sa {

namespace {

bool isLoopType(const std::string &kind) {
    return kind == "for_statement" || kind == "while_statement" || kind == "do_statement";
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

class Scanner {
public:
    Scanner(const std::string &path, long long threshold) : path_(path), threshold_(threshold) {}

    std::vector<Diagnostic> &diagnostics() { return diagnostics_; }

    void walkStmt(TSNode stmt, long long depth, bool &reported) {
        std::string kind = nodeKind(stmt);
        if (kind == "if_statement") {
            walkIf(stmt, depth, reported);
        } else if (isLoopType(kind)) {
            long long newDepth = depth + 1;
            maybeReport(stmt, newDepth, reported);
            TSNode body = childByField(stmt, "body");
            walkStmtOrBlock(body, !ts_node_is_null(body), newDepth, reported);
        } else if (kind == "switch_statement") {
            long long newDepth = depth + 1;
            maybeReport(stmt, newDepth, reported);
            TSNode body = childByField(stmt, "body");
            if (ts_node_is_null(body)) return;
            for (TSNode caseStmt : namedChildren(body)) {
                if (nodeKind(caseStmt) != "case_statement") continue;
                for (TSNode sub : caseBodyStmts(caseStmt)) walkStmt(sub, newDepth, reported);
            }
        }
    }

private:
    void maybeReport(TSNode node, long long depth, bool &reported) {
        if (depth > threshold_ && !reported) {
            reported = true;
            auto [line, col] = loc(node);
            Diagnostic d;
            d.path = path_;
            d.line = line;
            d.col = col;
            d.ruleId = Nesting::kRuleId;
            d.message =
                "Control flow nested " + std::to_string(depth) + " levels deep (threshold " +
                std::to_string(threshold_) + ")";
            diagnostics_.push_back(std::move(d));
        }
    }

    void walkStmtOrBlock(TSNode stmt, bool hasStmt, long long depth, bool &reported) {
        if (!hasStmt) return;
        if (nodeKind(stmt) == "compound_statement") {
            for (TSNode child : namedChildren(stmt)) walkStmt(child, depth, reported);
        } else {
            walkStmt(stmt, depth, reported);
        }
    }

    void walkIf(TSNode stmt, long long depth, bool &reported) {
        long long newDepth = depth + 1;
        maybeReport(stmt, newDepth, reported);
        TSNode consequence = childByField(stmt, "consequence");
        walkStmtOrBlock(consequence, !ts_node_is_null(consequence), newDepth, reported);

        TSNode alternative = childByField(stmt, "alternative");
        if (ts_node_is_null(alternative) || ts_node_named_child_count(alternative) == 0) return;
        TSNode inner = ts_node_named_child(alternative, 0);
        if (nodeKind(inner) == "if_statement") {
            walkIf(inner, depth, reported); // elif chains don't add a nesting level
        } else {
            walkStmtOrBlock(inner, true, newDepth, reported);
        }
    }

    std::string path_;
    long long threshold_;
    std::vector<Diagnostic> diagnostics_;
};

} // namespace

std::vector<Diagnostic> Nesting::check(const TSTree *tree, const std::string & /*source*/,
                                        const std::string &path, const Config &config) const {
    Scanner scanner(path, config.maxNesting);

    std::vector<TSNode> nodes;
    walk(ts_tree_root_node(tree), nodes);
    for (TSNode func : nodes) {
        if (nodeKind(func) != "function_definition") continue;
        TSNode body = childByField(func, "body");
        if (ts_node_is_null(body)) continue;

        bool reported = false;
        for (TSNode stmt : namedChildren(body)) scanner.walkStmt(stmt, 0, reported);
    }
    return scanner.diagnostics();
}

} // namespace sa
