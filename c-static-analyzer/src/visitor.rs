//! Shared traversal helpers reused across multiple rules.

use tree_sitter::Node;

/// 1-indexed line, 0-indexed column — matches tree-sitter's `Point` (both
/// 0-indexed) shifted to the same convention the Python implementation uses.
pub fn loc(node: &Node) -> (usize, usize) {
    let point = node.start_position();
    (point.row + 1, point.column)
}

/// Every descendant of `node`, depth-first, including `node` itself.
pub fn walk<'a>(node: Node<'a>, out: &mut Vec<Node<'a>>) {
    out.push(node);
    let mut cursor = node.walk();
    for child in node.children(&mut cursor) {
        walk(child, out);
    }
}

/// The declared name of a `function_definition` node, drilling through any
/// `pointer_declarator`/`function_declarator` wrapping to find the identifier.
pub fn function_name(func: &Node, source: &[u8]) -> String {
    let mut declarator = func.child_by_field_name("declarator");
    while let Some(node) = declarator {
        if node.kind() == "function_declarator" {
            return node
                .child_by_field_name("declarator")
                .and_then(|n| n.utf8_text(source).ok())
                .unwrap_or("<anonymous>")
                .to_string();
        }
        declarator = node.child_by_field_name("declarator");
    }
    "<anonymous>".to_string()
}
