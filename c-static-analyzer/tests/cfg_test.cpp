#include "cfg.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <tree_sitter/api.h>
#include <tree_sitter/tree-sitter-c.h>

#include "visitor.h"

namespace {

void expect(bool condition, const std::string &message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << "\n";
        std::abort();
    }
}

// Parses `source`, finds the named function's body, and builds its CFG.
// `tree`/`parser` are returned via out-params so the caller can keep them
// alive for as long as the CFG's TSNode statement references need to stay
// valid, then delete them once done.
sa::CFG buildFor(const std::string &source, const std::string &funcName, TSParser **parserOut, TSTree **treeOut) {
    TSParser *parser = ts_parser_new();
    ts_parser_set_language(parser, tree_sitter_c());
    TSTree *tree = ts_parser_parse_string(parser, nullptr, source.c_str(), static_cast<uint32_t>(source.size()));

    std::vector<TSNode> nodes;
    sa::walk(ts_tree_root_node(tree), nodes);
    for (TSNode node : nodes) {
        if (sa::nodeKind(node) != "function_definition") continue;
        if (sa::functionName(node, source) != funcName) continue;
        TSNode body = sa::childByField(node, "body");
        *parserOut = parser;
        *treeOut = tree;
        return sa::buildCFG(body, source);
    }
    expect(false, "function `" + funcName + "` not found");
    std::abort();
}

bool hasEdge(const sa::CFG &cfg, std::size_t from, std::size_t to, sa::EdgeKind kind) {
    for (const sa::CFGEdge &edge : cfg.block(from).successors) {
        if (edge.target == to && edge.kind == kind) return true;
    }
    return false;
}

// Whether `exit()` has an incoming Fallthrough edge from a block reachable
// from entry -- i.e. whether the function can fall off the end. This is
// exactly the predicate SA004 (missing-return) is built on.
bool canFallOffEnd(const sa::CFG &cfg) {
    const std::vector<bool> &reach = cfg.reachableFromEntry();
    for (std::size_t id = 0; id < cfg.blockCount(); ++id) {
        if (!reach[id]) continue;
        if (hasEdge(cfg, id, cfg.exit(), sa::EdgeKind::Fallthrough)) return true;
    }
    return false;
}

// Whether any block unreachable from entry contains a statement whose text
// contains `needle` -- the predicate SA005 (unreachable code) is built on.
bool hasUnreachableStatementContaining(const sa::CFG &cfg, const std::string &source, const std::string &needle) {
    const std::vector<bool> &reach = cfg.reachableFromEntry();
    for (std::size_t id = 0; id < cfg.blockCount(); ++id) {
        if (reach[id]) continue;
        for (TSNode stmt : cfg.block(id).statements) {
            if (sa::nodeText(stmt, source).find(needle) != std::string::npos) return true;
        }
    }
    return false;
}

} // namespace

int main() {
    // A bare `if` with no `else` can fall off the end.
    {
        TSParser *parser;
        TSTree *tree;
        sa::CFG cfg = buildFor("const char *f(int x) {\n"
                                "    if (x > 0) {\n"
                                "        return \"positive\";\n"
                                "    }\n"
                                "}\n",
                                "f", &parser, &tree);
        expect(canFallOffEnd(cfg), "a bare if with no else should be able to fall off the end");
        ts_tree_delete(tree);
        ts_parser_delete(parser);
    }

    // An exhaustive if/else (both branches return) cannot fall off the end.
    {
        TSParser *parser;
        TSTree *tree;
        sa::CFG cfg = buildFor("const char *f(int x) {\n"
                                "    if (x > 0) {\n"
                                "        return \"positive\";\n"
                                "    } else {\n"
                                "        return \"non-positive\";\n"
                                "    }\n"
                                "}\n",
                                "f", &parser, &tree);
        expect(!canFallOffEnd(cfg), "an exhaustive if/else should not be able to fall off the end");
        ts_tree_delete(tree);
        ts_parser_delete(parser);
    }

    // An infinite loop (`while (1)`) with no break can't fall off the end.
    {
        TSParser *parser;
        TSTree *tree;
        sa::CFG cfg = buildFor("int f(void) {\n"
                                "    while (1) {\n"
                                "        handle();\n"
                                "    }\n"
                                "}\n",
                                "f", &parser, &tree);
        expect(!canFallOffEnd(cfg), "an infinite loop without break should not be able to fall off the end");
        ts_tree_delete(tree);
        ts_parser_delete(parser);
    }

    // The same infinite loop with a `break` reaching past it can fall off
    // the end (through the point the break jumps to).
    {
        TSParser *parser;
        TSTree *tree;
        sa::CFG cfg = buildFor("int f(int n) {\n"
                                "    while (1) {\n"
                                "        if (n > 0) {\n"
                                "            break;\n"
                                "        }\n"
                                "    }\n"
                                "}\n",
                                "f", &parser, &tree);
        expect(canFallOffEnd(cfg), "an infinite loop with a break should be able to fall off the end");
        ts_tree_delete(tree);
        ts_parser_delete(parser);
    }

    // A `for` loop with no break, and a genuine post-loop statement, has
    // that statement reachable via the loop's False edge.
    {
        TSParser *parser;
        TSTree *tree;
        sa::CFG cfg = buildFor("int f(int n) {\n"
                                "    for (int i = 0; i < n; i++) {\n"
                                "        handle();\n"
                                "    }\n"
                                "    return 0;\n"
                                "}\n",
                                "f", &parser, &tree);
        expect(!canFallOffEnd(cfg), "a for loop followed by a return should not be able to fall off the end");
        ts_tree_delete(tree);
        ts_parser_delete(parser);
    }

    // Code after `return` in the same block is unreachable.
    {
        TSParser *parser;
        TSTree *tree;
        sa::CFG cfg = buildFor("int f(void) {\n"
                                "    return 1;\n"
                                "    handle();\n"
                                "}\n",
                                "f", &parser, &tree);
        expect(hasUnreachableStatementContaining(cfg, "int f(void) {\n    return 1;\n    handle();\n}\n", "handle"),
               "code after return should be unreachable");
        ts_tree_delete(tree);
        ts_parser_delete(parser);
    }

    // Reachable code (after an if with no else) is not flagged unreachable.
    {
        TSParser *parser;
        TSTree *tree;
        const std::string source = "int f(int x) {\n"
                                    "    if (x) {\n"
                                    "        return 1;\n"
                                    "    }\n"
                                    "    return 2;\n"
                                    "}\n";
        sa::CFG cfg = buildFor(source, "f", &parser, &tree);
        expect(!hasUnreachableStatementContaining(cfg, source, "return 2"),
               "code reachable via the if's false edge should not be flagged unreachable");
        ts_tree_delete(tree);
        ts_parser_delete(parser);
    }

    // Switch fallthrough: a case with no break flows into the next case's
    // block, so a statement in the next case is not unreachable even
    // though it's also reachable directly by matching that case.
    {
        TSParser *parser;
        TSTree *tree;
        const std::string source = "int f(int x) {\n"
                                    "    switch (x) {\n"
                                    "    case 1:\n"
                                    "        handle();\n"
                                    "    case 2:\n"
                                    "        other();\n"
                                    "        break;\n"
                                    "    }\n"
                                    "    return 0;\n"
                                    "}\n";
        sa::CFG cfg = buildFor(source, "f", &parser, &tree);
        expect(!hasUnreachableStatementContaining(cfg, source, "other"),
               "a fallthrough case's statements should be reachable");
        ts_tree_delete(tree);
        ts_parser_delete(parser);
    }

    // Code after `return` inside a case is still unreachable (no
    // fallthrough survives a return).
    {
        TSParser *parser;
        TSTree *tree;
        const std::string source = "int f(int x) {\n"
                                    "    switch (x) {\n"
                                    "    case 1:\n"
                                    "        return 1;\n"
                                    "        return 2;\n"
                                    "    }\n"
                                    "    return 0;\n"
                                    "}\n";
        sa::CFG cfg = buildFor(source, "f", &parser, &tree);
        expect(hasUnreachableStatementContaining(cfg, source, "return 2"),
               "code after return inside a case should be unreachable");
        ts_tree_delete(tree);
        ts_parser_delete(parser);
    }

    // A forward `goto` reaches its label, so code between the goto and the
    // label is unreachable but the labeled code itself is not.
    {
        TSParser *parser;
        TSTree *tree;
        const std::string source = "int f(int x) {\n"
                                    "    goto done;\n"
                                    "    handle();\n"
                                    "done:\n"
                                    "    return 0;\n"
                                    "}\n";
        sa::CFG cfg = buildFor(source, "f", &parser, &tree);
        expect(hasUnreachableStatementContaining(cfg, source, "handle"),
               "code between a goto and its forward label should be unreachable");
        expect(!hasUnreachableStatementContaining(cfg, source, "return 0"),
               "the goto's label target should be reachable");
        ts_tree_delete(tree);
        ts_parser_delete(parser);
    }

    // A backward `goto` (hand-rolled loop) reaches code between the label
    // and the goto on every iteration after the first, so it's reachable.
    {
        TSParser *parser;
        TSTree *tree;
        const std::string source = "int f(int n) {\n"
                                    "top:\n"
                                    "    handle();\n"
                                    "    if (n > 0) {\n"
                                    "        n = n - 1;\n"
                                    "        goto top;\n"
                                    "    }\n"
                                    "    return 0;\n"
                                    "}\n";
        sa::CFG cfg = buildFor(source, "f", &parser, &tree);
        expect(!hasUnreachableStatementContaining(cfg, source, "handle"), "a backward goto's label should be reachable");
        expect(!canFallOffEnd(cfg), "the function always returns, so it should not be able to fall off the end");
        ts_tree_delete(tree);
        ts_parser_delete(parser);
    }

    std::cout << "cfg_test: all checks passed\n";
    return 0;
}
