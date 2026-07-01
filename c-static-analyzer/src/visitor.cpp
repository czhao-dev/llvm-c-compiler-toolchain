#include "visitor.h"

#include <cstring>

namespace sa {

TSNode childByField(TSNode node, const char *fieldName) {
    return ts_node_child_by_field_name(node, fieldName, static_cast<uint32_t>(std::strlen(fieldName)));
}

std::vector<TSNode> childrenByField(TSNode node, const char *fieldName) {
    std::vector<TSNode> result;
    TSTreeCursor cursor = ts_tree_cursor_new(node);
    if (ts_tree_cursor_goto_first_child(&cursor)) {
        do {
            const char *currentField = ts_tree_cursor_current_field_name(&cursor);
            if (currentField != nullptr && std::strcmp(currentField, fieldName) == 0) {
                result.push_back(ts_tree_cursor_current_node(&cursor));
            }
        } while (ts_tree_cursor_goto_next_sibling(&cursor));
    }
    ts_tree_cursor_delete(&cursor);
    return result;
}

std::vector<TSNode> namedChildren(TSNode node) {
    std::vector<TSNode> result;
    uint32_t count = ts_node_named_child_count(node);
    result.reserve(count);
    for (uint32_t i = 0; i < count; ++i) result.push_back(ts_node_named_child(node, i));
    return result;
}

std::vector<TSNode> children(TSNode node) {
    std::vector<TSNode> result;
    uint32_t count = ts_node_child_count(node);
    result.reserve(count);
    for (uint32_t i = 0; i < count; ++i) result.push_back(ts_node_child(node, i));
    return result;
}

std::string nodeKind(TSNode node) { return ts_node_type(node); }

std::string nodeText(TSNode node, const std::string &source) {
    uint32_t start = ts_node_start_byte(node);
    uint32_t end = ts_node_end_byte(node);
    return source.substr(start, end - start);
}

std::pair<std::size_t, std::size_t> loc(TSNode node) {
    TSPoint point = ts_node_start_point(node);
    return {static_cast<std::size_t>(point.row) + 1, static_cast<std::size_t>(point.column)};
}

void walk(TSNode node, std::vector<TSNode> &out) {
    out.push_back(node);
    uint32_t count = ts_node_child_count(node);
    for (uint32_t i = 0; i < count; ++i) walk(ts_node_child(node, i), out);
}

std::string functionName(TSNode func, const std::string &source) {
    TSNode declarator = childByField(func, "declarator");
    while (!ts_node_is_null(declarator)) {
        if (nodeKind(declarator) == "function_declarator") {
            TSNode inner = childByField(declarator, "declarator");
            return ts_node_is_null(inner) ? "<anonymous>" : nodeText(inner, source);
        }
        declarator = childByField(declarator, "declarator");
    }
    return "<anonymous>";
}

} // namespace sa
