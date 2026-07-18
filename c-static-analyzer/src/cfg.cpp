#include "cfg.h"

#include <queue>
#include <unordered_map>

#include "visitor.h"

namespace sa {

namespace {

// Sentinel meaning "no block currently represents the continuation of
// control flow" -- used only after a return/break/continue/goto, which
// don't have a natural syntactic "what comes after" location the way an
// if/loop's merge/after block does. The next statement (if any) gets a
// fresh, initially-predecessor-less block allocated lazily; if it's never
// linked from anywhere, it's correctly unreachable.
constexpr std::size_t kUnreachable = static_cast<std::size_t>(-1);

std::string trimParensAndSpaces(const std::string &s) {
    auto isTrimChar = [](char c) { return c == '(' || c == ')' || c == ' '; };
    std::size_t start = 0;
    std::size_t end = s.size();
    while (start < end && isTrimChar(s[start])) ++start;
    while (end > start && isTrimChar(s[end - 1])) --end;
    return s.substr(start, end - start);
}

bool isLiteralTrueCondition(TSNode condition, const std::string &source) {
    if (ts_node_is_null(condition)) return true; // e.g. `for (;;)`
    std::string text = trimParensAndSpaces(nodeText(condition, source));
    return text == "1" || text == "true";
}

// A case/default label's body statements, excluding the `value` field
// (the `case <expr>` literal itself isn't a body statement).
std::vector<TSNode> caseBodyStmts(TSNode caseStmt) {
    TSNode value = childByField(caseStmt, "value");
    std::vector<TSNode> result;
    for (TSNode child : namedChildren(caseStmt)) {
        if (!ts_node_is_null(value) && child.id == value.id) continue;
        result.push_back(child);
    }
    return result;
}

class Builder {
public:
    explicit Builder(const std::string &source) : source_(source) {
        entry_ = newBlock();
        exit_ = newBlock();
    }

    CFG build(TSNode functionBody) {
        collectLabels(functionBody);
        std::size_t end = processCompound(functionBody, entry_);
        if (end != kUnreachable) {
            addEdge(end, exit_, EdgeKind::Fallthrough);
        }
        return CFG(std::move(blocks_), entry_, exit_);
    }

private:
    const std::string &source_;
    std::vector<BasicBlock> blocks_;
    std::size_t entry_;
    std::size_t exit_;
    std::unordered_map<std::string, std::size_t> labelBlocks_;
    std::vector<std::size_t> breakTargets_;
    std::vector<std::size_t> continueTargets_;

    std::size_t newBlock() {
        std::size_t id = blocks_.size();
        blocks_.push_back(BasicBlock{id, {}, {}, {}});
        return id;
    }

    void addEdge(std::size_t from, std::size_t to, EdgeKind kind) {
        blocks_[from].successors.push_back(CFGEdge{to, kind});
        blocks_[to].predecessors.push_back(from);
    }

    // Every `labeled_statement` needs its target block allocated before
    // traversal reaches it, so a `goto` appearing earlier in source order
    // than its label can still be wired up.
    void collectLabels(TSNode functionBody) {
        std::vector<TSNode> nodes;
        walk(functionBody, nodes);
        for (TSNode node : nodes) {
            if (nodeKind(node) != "labeled_statement") continue;
            TSNode labelNode = childByField(node, "label");
            if (ts_node_is_null(labelNode)) continue;
            labelBlocks_.emplace(nodeText(labelNode, source_), newBlock());
        }
    }

    std::size_t processCompound(TSNode compound, std::size_t cur) {
        return processStmtList(namedChildren(compound), cur);
    }

    std::size_t processStmtList(const std::vector<TSNode> &stmts, std::size_t cur) {
        for (TSNode stmt : stmts) {
            if (cur == kUnreachable) cur = newBlock();
            cur = processStmt(stmt, cur);
        }
        return cur;
    }

    std::size_t processStmt(TSNode stmt, std::size_t cur) {
        std::string kind = nodeKind(stmt);

        if (kind == "compound_statement") return processCompound(stmt, cur);
        if (kind == "labeled_statement") return processLabeled(stmt, cur);
        if (kind == "if_statement") return processIf(stmt, cur);
        if (kind == "while_statement") return processWhile(stmt, cur);
        if (kind == "do_statement") return processDoWhile(stmt, cur);
        if (kind == "for_statement") return processFor(stmt, cur);
        if (kind == "switch_statement") return processSwitch(stmt, cur);

        if (kind == "return_statement") {
            blocks_[cur].statements.push_back(stmt);
            addEdge(cur, exit_, EdgeKind::Unconditional);
            return kUnreachable;
        }
        if (kind == "break_statement") {
            blocks_[cur].statements.push_back(stmt);
            if (!breakTargets_.empty()) addEdge(cur, breakTargets_.back(), EdgeKind::Unconditional);
            return kUnreachable;
        }
        if (kind == "continue_statement") {
            blocks_[cur].statements.push_back(stmt);
            if (!continueTargets_.empty()) addEdge(cur, continueTargets_.back(), EdgeKind::Unconditional);
            return kUnreachable;
        }
        if (kind == "goto_statement") {
            blocks_[cur].statements.push_back(stmt);
            TSNode labelNode = childByField(stmt, "label");
            if (!ts_node_is_null(labelNode)) {
                auto it = labelBlocks_.find(nodeText(labelNode, source_));
                if (it != labelBlocks_.end()) addEdge(cur, it->second, EdgeKind::Unconditional);
            }
            return kUnreachable;
        }

        // Anything else (declarations, expression statements, assignments,
        // and any statement kind this tool doesn't specifically model) is
        // an opaque straight-line statement with no control-flow effect.
        blocks_[cur].statements.push_back(stmt);
        return cur;
    }

    std::size_t processLabeled(TSNode stmt, std::size_t cur) {
        TSNode labelNode = childByField(stmt, "label");
        std::size_t labelBlock = labelBlocks_.at(nodeText(labelNode, source_));
        addEdge(cur, labelBlock, EdgeKind::Fallthrough);

        // `label ':' (declaration | statement)` -- the label is the first
        // named child, the labeled declaration/statement is the last.
        std::vector<TSNode> named = namedChildren(stmt);
        TSNode inner = named.back();
        return processStmt(inner, labelBlock);
    }

    std::size_t processIf(TSNode stmt, std::size_t cur) {
        blocks_[cur].statements.push_back(stmt);
        TSNode consequence = childByField(stmt, "consequence");
        TSNode alternative = childByField(stmt, "alternative");

        std::size_t mergeBlock = newBlock();

        std::size_t thenBlock = newBlock();
        addEdge(cur, thenBlock, EdgeKind::True);
        std::size_t thenEnd = processStmt(consequence, thenBlock);
        if (thenEnd != kUnreachable) addEdge(thenEnd, mergeBlock, EdgeKind::Fallthrough);

        if (!ts_node_is_null(alternative)) {
            std::size_t elseBlock = newBlock();
            addEdge(cur, elseBlock, EdgeKind::False);
            // `alternative` is an `else_clause`; its one named child is the
            // actual else-statement (possibly another if_statement for
            // `else if`).
            std::vector<TSNode> elseChildren = namedChildren(alternative);
            TSNode elseStmt = elseChildren.empty() ? alternative : elseChildren.front();
            std::size_t elseEnd = processStmt(elseStmt, elseBlock);
            if (elseEnd != kUnreachable) addEdge(elseEnd, mergeBlock, EdgeKind::Fallthrough);
        } else {
            addEdge(cur, mergeBlock, EdgeKind::False);
        }

        return mergeBlock;
    }

    std::size_t processWhile(TSNode stmt, std::size_t cur) {
        TSNode condition = childByField(stmt, "condition");
        TSNode body = childByField(stmt, "body");

        std::size_t condBlock = newBlock();
        addEdge(cur, condBlock, EdgeKind::Fallthrough);
        blocks_[condBlock].statements.push_back(stmt);

        std::size_t bodyBlock = newBlock();
        addEdge(condBlock, bodyBlock, EdgeKind::True);

        std::size_t afterBlock = newBlock();
        if (!isLiteralTrueCondition(condition, source_)) {
            addEdge(condBlock, afterBlock, EdgeKind::False);
        }

        breakTargets_.push_back(afterBlock);
        continueTargets_.push_back(condBlock);
        std::size_t bodyEnd = processStmt(body, bodyBlock);
        breakTargets_.pop_back();
        continueTargets_.pop_back();
        if (bodyEnd != kUnreachable) addEdge(bodyEnd, condBlock, EdgeKind::Fallthrough);

        return afterBlock;
    }

    std::size_t processDoWhile(TSNode stmt, std::size_t cur) {
        TSNode body = childByField(stmt, "body");
        TSNode condition = childByField(stmt, "condition");

        std::size_t bodyBlock = newBlock();
        addEdge(cur, bodyBlock, EdgeKind::Fallthrough);

        std::size_t condBlock = newBlock();
        std::size_t afterBlock = newBlock();

        breakTargets_.push_back(afterBlock);
        continueTargets_.push_back(condBlock);
        std::size_t bodyEnd = processStmt(body, bodyBlock);
        breakTargets_.pop_back();
        continueTargets_.pop_back();
        if (bodyEnd != kUnreachable) addEdge(bodyEnd, condBlock, EdgeKind::Fallthrough);

        blocks_[condBlock].statements.push_back(stmt);
        addEdge(condBlock, bodyBlock, EdgeKind::True);
        if (!isLiteralTrueCondition(condition, source_)) {
            addEdge(condBlock, afterBlock, EdgeKind::False);
        }

        return afterBlock;
    }

    std::size_t processFor(TSNode stmt, std::size_t cur) {
        TSNode initializer = childByField(stmt, "initializer");
        TSNode condition = childByField(stmt, "condition");
        TSNode update = childByField(stmt, "update");
        TSNode body = childByField(stmt, "body");

        if (!ts_node_is_null(initializer)) blocks_[cur].statements.push_back(initializer);

        std::size_t condBlock = newBlock();
        addEdge(cur, condBlock, EdgeKind::Fallthrough);
        blocks_[condBlock].statements.push_back(stmt);

        std::size_t bodyBlock = newBlock();
        addEdge(condBlock, bodyBlock, EdgeKind::True);

        std::size_t afterBlock = newBlock();
        if (!isLiteralTrueCondition(condition, source_)) {
            addEdge(condBlock, afterBlock, EdgeKind::False);
        }

        std::size_t updateBlock = newBlock();
        if (!ts_node_is_null(update)) blocks_[updateBlock].statements.push_back(update);

        breakTargets_.push_back(afterBlock);
        continueTargets_.push_back(updateBlock);
        std::size_t bodyEnd = processStmt(body, bodyBlock);
        breakTargets_.pop_back();
        continueTargets_.pop_back();
        if (bodyEnd != kUnreachable) addEdge(bodyEnd, updateBlock, EdgeKind::Fallthrough);
        addEdge(updateBlock, condBlock, EdgeKind::Fallthrough);

        return afterBlock;
    }

    std::size_t processSwitch(TSNode stmt, std::size_t cur) {
        TSNode body = childByField(stmt, "body");
        blocks_[cur].statements.push_back(stmt);

        std::size_t afterBlock = newBlock();
        breakTargets_.push_back(afterBlock);

        std::size_t prevEnd = kUnreachable;
        bool hasDefault = false;

        for (TSNode c : namedChildren(body)) {
            if (nodeKind(c) != "case_statement") continue;
            std::size_t caseBlock = newBlock();
            addEdge(cur, caseBlock, EdgeKind::Unconditional); // dispatch: this case may match
            if (prevEnd != kUnreachable) addEdge(prevEnd, caseBlock, EdgeKind::Fallthrough);
            if (ts_node_is_null(childByField(c, "value"))) hasDefault = true;
            prevEnd = processStmtList(caseBodyStmts(c), caseBlock);
        }
        if (prevEnd != kUnreachable) addEdge(prevEnd, afterBlock, EdgeKind::Fallthrough);
        if (!hasDefault) addEdge(cur, afterBlock, EdgeKind::Unconditional); // no case matches

        breakTargets_.pop_back();
        return afterBlock;
    }
};

} // namespace

const std::vector<bool> &CFG::reachableFromEntry() const {
    if (reachableComputed_) return reachableCache_;

    reachableCache_.assign(blocks_.size(), false);
    std::queue<std::size_t> pending;
    reachableCache_[entry_] = true;
    pending.push(entry_);
    while (!pending.empty()) {
        std::size_t id = pending.front();
        pending.pop();
        for (const CFGEdge &edge : blocks_[id].successors) {
            if (reachableCache_[edge.target]) continue;
            reachableCache_[edge.target] = true;
            pending.push(edge.target);
        }
    }

    reachableComputed_ = true;
    return reachableCache_;
}

CFG buildCFG(TSNode functionBody, const std::string &source) {
    Builder builder(source);
    return builder.build(functionBody);
}

} // namespace sa
