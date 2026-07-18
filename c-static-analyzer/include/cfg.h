#ifndef SA_CFG_H
#define SA_CFG_H

#include <cstddef>
#include <string>
#include <vector>

#include <tree_sitter/api.h>

namespace sa {

// How control reaches an edge's target block.
enum class EdgeKind {
    Unconditional, // straight-line flow, or an explicit jump (return/break/continue/goto)
    True,          // an if/while/for/do-while condition evaluating true
    False,         // an if/while/for/do-while condition evaluating false
    Fallthrough,   // falls from one statement/case into the next without a jump
};

struct CFGEdge {
    std::size_t target;
    EdgeKind kind;
};

// One straight-line run of statements: no branch enters or leaves it except
// at its boundaries. `statements` holds the original tree-sitter nodes in
// source order (empty for the synthetic entry/exit blocks, and for a block
// that exists only to be a branch target with no statements of its own,
// e.g. an empty `else`-less `if`'s merge point before anything follows it).
struct BasicBlock {
    std::size_t id = 0;
    std::vector<TSNode> statements;
    std::vector<CFGEdge> successors;
    std::vector<std::size_t> predecessors;
};

// A control-flow graph for a single function body: a synthetic entry block,
// a synthetic exit block every `return` (and, if the function can fall off
// the end, the final statement) reaches, and one block per straight-line
// run of statements in between.
//
// This is the data structure the "should support" side of the toolchain
// spec calls for: SA004/SA005/SA006 are graph traversals over it rather
// than ad hoc recursive AST pattern matching. See buildCFG below for the
// construction algorithm.
class CFG {
public:
    explicit CFG(std::vector<BasicBlock> blocks, std::size_t entry, std::size_t exit)
        : blocks_(std::move(blocks)), entry_(entry), exit_(exit) {}

    const BasicBlock &block(std::size_t id) const { return blocks_[id]; }
    std::size_t entry() const { return entry_; }
    std::size_t exit() const { return exit_; }
    std::size_t blockCount() const { return blocks_.size(); }

    // Every block reachable from `entry()` by following successor edges,
    // indexed by block id. Computed on first use and cached.
    const std::vector<bool> &reachableFromEntry() const;

private:
    std::vector<BasicBlock> blocks_;
    std::size_t entry_;
    std::size_t exit_;
    mutable std::vector<bool> reachableCache_;
    mutable bool reachableComputed_ = false;
};

// Builds a CFG for one function's body (a `compound_statement` node, e.g.
// `childByField(func, "body")`). Handles if/else, while, do-while, for,
// switch/case/default fallthrough, break/continue, goto/labels, and return.
// Statement kinds outside that set (declarations, expression statements,
// assignments, and anything else this tool doesn't specifically model) are
// treated as opaque straight-line statements with no control-flow effect
// of their own.
CFG buildCFG(TSNode functionBody, const std::string &source);

} // namespace sa

#endif // SA_CFG_H
