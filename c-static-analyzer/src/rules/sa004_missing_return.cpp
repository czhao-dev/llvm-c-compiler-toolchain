#include "rules/sa004_missing_return.h"

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

std::vector<TSNode> blockStmts(TSNode node, bool hasNode) {
    if (!hasNode) return {};
    if (nodeKind(node) == "compound_statement") return namedChildren(node);
    return {node};
}

bool isOneOf(const std::string &kind, std::initializer_list<const char *> options) {
    for (const char *option : options) {
        if (kind == option) return true;
    }
    return false;
}

std::string trimParensAndSpaces(const std::string &s) {
    auto isTrimChar = [](char c) { return c == '(' || c == ')' || c == ' '; };
    std::size_t start = 0;
    std::size_t end = s.size();
    while (start < end && isTrimChar(s[start])) ++start;
    while (end > start && isTrimChar(s[end - 1])) --end;
    return s.substr(start, end - start);
}

bool stmtAlwaysExits(TSNode stmt, const std::string &source);

bool alwaysExits(TSNode node, bool hasNode, const std::string &source) {
    std::vector<TSNode> stmts = blockStmts(node, hasNode);
    if (stmts.empty()) return false;
    return stmtAlwaysExits(stmts.back(), source);
}

// Whether a `break` targeting THIS loop/switch appears in `stmts` (not
// crossing into a nested loop/switch's own scope).
bool containsBreak(const std::vector<TSNode> &stmts) {
    for (TSNode stmt : stmts) {
        std::string kind = nodeKind(stmt);
        if (kind == "break_statement") return true;
        if (isOneOf(kind, {"for_statement", "while_statement", "do_statement", "switch_statement"})) {
            continue;
        }
        if (kind == "compound_statement") {
            if (containsBreak(namedChildren(stmt))) return true;
            continue;
        }
        for (const char *field : {"consequence", "body"}) {
            TSNode child = childByField(stmt, field);
            if (!ts_node_is_null(child) && containsBreak(blockStmts(child, true))) return true;
        }
        TSNode alternative = childByField(stmt, "alternative");
        if (!ts_node_is_null(alternative) && ts_node_named_child_count(alternative) > 0) {
            TSNode inner = ts_node_named_child(alternative, 0);
            if (containsBreak(blockStmts(inner, true))) return true;
        }
    }
    return false;
}

bool stmtAlwaysExits(TSNode stmt, const std::string &source) {
    std::string kind = nodeKind(stmt);
    if (kind == "return_statement") return true;

    if (kind == "if_statement") {
        TSNode alternative = childByField(stmt, "alternative");
        if (ts_node_is_null(alternative) || ts_node_named_child_count(alternative) == 0) return false;
        TSNode inner = ts_node_named_child(alternative, 0);
        TSNode consequence = childByField(stmt, "consequence");
        return alwaysExits(consequence, !ts_node_is_null(consequence), source) &&
               alwaysExits(inner, true, source);
    }

    if (isOneOf(kind, {"while_statement", "do_statement"})) {
        TSNode condition = childByField(stmt, "condition");
        bool isInfinite = false;
        if (!ts_node_is_null(condition)) {
            std::string text = trimParensAndSpaces(nodeText(condition, source));
            isInfinite = (text == "1" || text == "true");
        }
        TSNode body = childByField(stmt, "body");
        return isInfinite && !containsBreak(blockStmts(body, !ts_node_is_null(body)));
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
        if (!alwaysExits(body, !ts_node_is_null(body), source)) {
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
