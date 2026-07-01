#ifndef SA_VISITOR_H
#define SA_VISITOR_H

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include <tree_sitter/api.h>

namespace sa {

// Small tree-sitter C-API helpers shared across the analyzer and every
// rule (the C API is lower-level than the tree-sitter Rust bindings the
// original tool used, so these exist purely to keep call sites readable).

// The child of `node` with the given grammar field name, or a null TSNode
// (check with ts_node_is_null()) if there is none.
TSNode childByField(TSNode node, const char *fieldName);

// Every child of `node` (named or not) whose field name is `fieldName`,
// in order. Used where a single field name can repeat, e.g. the several
// `declarator` children of `int a, b;`.
std::vector<TSNode> childrenByField(TSNode node, const char *fieldName);

// All named children of `node`, in order.
std::vector<TSNode> namedChildren(TSNode node);

// All children of `node` (named and anonymous), in order.
std::vector<TSNode> children(TSNode node);

// The grammar node kind, e.g. "if_statement".
std::string nodeKind(TSNode node);

// The exact source text spanned by `node`.
std::string nodeText(TSNode node, const std::string &source);

// 1-indexed line, 0-indexed column — matches tree-sitter's TSPoint (both
// 0-indexed) shifted to the convention the original tool used.
std::pair<std::size_t, std::size_t> loc(TSNode node);

// Every descendant of `node`, depth-first pre-order, including `node`
// itself (both named and anonymous children).
void walk(TSNode node, std::vector<TSNode> &out);

// The declared name of a `function_definition` node, drilling through any
// `pointer_declarator`/`function_declarator` wrapping to find the
// identifier. Returns "<anonymous>" if none is found.
std::string functionName(TSNode func, const std::string &source);

} // namespace sa

#endif // SA_VISITOR_H
