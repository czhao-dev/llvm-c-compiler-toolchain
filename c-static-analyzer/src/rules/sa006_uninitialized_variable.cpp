#include "rules/sa006_uninitialized_variable.h"

#include <algorithm>
#include <optional>
#include <unordered_map>

#include "cfg.h"
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

struct Declaration {
    std::string name;
    std::size_t block = 0;
    std::size_t stmtIndex = 0;
    TSNode nameNode{};
};

// Every local declared without an initializer (non-array, not
// underscore-prefixed), keyed by name -- first declaration site wins,
// matching the flat per-function tracking this rule has always used (a
// shadowing redeclaration in a nested scope is not separately tracked).
std::vector<Declaration> collectDeclarations(const CFG &cfg, const std::string &source) {
    std::unordered_map<std::string, bool> seen;
    std::vector<Declaration> result;
    for (std::size_t b = 0; b < cfg.blockCount(); ++b) {
        const std::vector<TSNode> &stmts = cfg.block(b).statements;
        for (std::size_t i = 0; i < stmts.size(); ++i) {
            if (nodeKind(stmts[i]) != "declaration") continue;
            for (TSNode declarator : childrenByField(stmts[i], "declarator")) {
                if (nodeKind(declarator) == "init_declarator") continue; // has an initializer
                DeclaratorInfo info = baseIdentifierInfo(declarator);
                if (ts_node_is_null(info.nameNode) || info.isArray) continue;
                std::string name = nodeText(info.nameNode, source);
                if (name.empty() || name.front() == '_') continue;
                if (!seen.emplace(name, true).second) continue;
                result.push_back(Declaration{name, b, i, info.nameNode});
            }
        }
    }
    return result;
}

// Scans a block's statements from `fromIndex` onward for every reference
// to `varName`, in source order. Returns the first read encountered while
// still (possibly) uninitialized, if any, and whether the variable is
// still (possibly) uninitialized at the end of the block.
struct ScanResult {
    bool stillUninit;
    std::optional<TSNode> badRead;
};

// The CFG builder stores an if/while/do/for/switch statement's own node in
// the block representing its condition check, but its branches/body are
// represented by *separate* CFG blocks -- a full-subtree walk here would
// double-count references that structurally belong to those other blocks
// (and, worse, let a write on one branch incorrectly mark the variable
// initialized on every path). Only the condition expression is "owned" by
// this block; everything else about these statement kinds is scanned when
// their own block is visited.
void collectOwnedIdentifiers(TSNode node, std::vector<TSNode> &out) {
    static const std::unordered_map<std::string, bool> kConditionOwners = {
        {"if_statement", true}, {"while_statement", true}, {"do_statement", true},
        {"for_statement", true}, {"switch_statement", true},
    };
    if (kConditionOwners.count(nodeKind(node)) > 0) {
        TSNode condition = childByField(node, "condition");
        if (!ts_node_is_null(condition)) walk(condition, out);
        return;
    }
    walk(node, out);
}

ScanResult scanBlock(const BasicBlock &block, std::size_t fromIndex, bool uninit, const std::string &varName,
                      const std::string &source) {
    for (std::size_t i = fromIndex; i < block.statements.size(); ++i) {
        std::vector<TSNode> nodes;
        collectOwnedIdentifiers(block.statements[i], nodes);
        for (TSNode node : nodes) {
            if (nodeKind(node) != "identifier" || nodeText(node, source) != varName) continue;
            if (isWriteReference(node, source)) {
                uninit = false;
            } else if (uninit) {
                return ScanResult{true, node};
            }
        }
    }
    return ScanResult{uninit, std::nullopt};
}

// "May be uninitialized" dataflow: explores every path from `decl`'s
// declaration forward through the CFG, tracking whether the variable is
// still possibly-uninitialized. Since the per-block state is a single
// boolean, visiting each block at most once per state value is enough to
// cover every reachable (block, state) pair -- equivalent to a fixed-point
// forward dataflow with an OR (may, not must) join at merge points, which
// is what a real compiler's "may be used uninitialized" warning uses.
std::optional<TSNode> findBadRead(const CFG &cfg, const Declaration &decl, const std::string &source) {
    std::vector<bool> visitedUninit(cfg.blockCount(), false);
    std::vector<bool> visitedInit(cfg.blockCount(), false);

    struct WorkItem {
        std::size_t block;
        std::size_t fromIndex;
        bool uninit;
    };
    std::vector<WorkItem> stack;
    stack.push_back(WorkItem{decl.block, decl.stmtIndex + 1, true});

    std::vector<TSNode> badReads;
    while (!stack.empty()) {
        WorkItem item = stack.back();
        stack.pop_back();

        std::vector<bool> &visited = item.uninit ? visitedUninit : visitedInit;
        if (visited[item.block]) continue;
        visited[item.block] = true;

        ScanResult result = scanBlock(cfg.block(item.block), item.fromIndex, item.uninit, decl.name, source);
        if (result.badRead.has_value()) {
            badReads.push_back(*result.badRead);
            continue; // this path already has a violation; no need to go further
        }
        for (const CFGEdge &edge : cfg.block(item.block).successors) {
            stack.push_back(WorkItem{edge.target, 0, result.stillUninit});
        }
    }

    if (badReads.empty()) return std::nullopt;
    return *std::min_element(badReads.begin(), badReads.end(),
                              [](TSNode a, TSNode b) { return loc(a) < loc(b); });
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

        CFG cfg = buildCFG(body, source);
        for (const Declaration &decl : collectDeclarations(cfg, source)) {
            std::optional<TSNode> badRead = findBadRead(cfg, decl, source);
            if (!badRead.has_value()) continue;

            auto [line, col] = loc(*badRead);
            Diagnostic d;
            d.path = path;
            d.line = line;
            d.col = col;
            d.ruleId = kRuleId;
            d.message = "Local variable `" + decl.name + "` may be used before being initialized";
            diagnostics.push_back(std::move(d));
        }
    }

    std::sort(diagnostics.begin(), diagnostics.end());
    return diagnostics;
}

} // namespace sa
