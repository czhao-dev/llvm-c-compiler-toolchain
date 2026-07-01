use tree_sitter::{Node, Tree};

use super::Rule;
use crate::config::Config;
use crate::diagnostics::Diagnostic;
use crate::visitor::{function_name, loc, walk};

pub struct MissingReturn;

impl MissingReturn {
    pub const RULE_ID: &'static str = "SA004";
}

const INFINITE_LOOP_TYPES: &[&str] = &["while_statement", "do_statement"];
const LOOP_OR_SWITCH_TYPES: &[&str] = &[
    "for_statement",
    "while_statement",
    "do_statement",
    "switch_statement",
];

fn is_void_function(func: &Node, source: &[u8]) -> bool {
    let Some(type_node) = func.child_by_field_name("type") else {
        return false;
    };
    if type_node.utf8_text(source) != Ok("void") {
        return false;
    }
    let mut declarator = func.child_by_field_name("declarator");
    while let Some(node) = declarator {
        if node.kind() == "function_declarator" {
            return true;
        }
        if node.kind() == "pointer_declarator" {
            return false; // returns void*, which is a value
        }
        declarator = node.child_by_field_name("declarator");
    }
    false
}

fn block_stmts<'a>(node: Option<Node<'a>>) -> Vec<Node<'a>> {
    let Some(node) = node else {
        return Vec::new();
    };
    if node.kind() == "compound_statement" {
        let mut cursor = node.walk();
        node.named_children(&mut cursor).collect()
    } else {
        vec![node]
    }
}

/// Whether a `break` targeting THIS loop/switch appears in `stmts` (not crossing a nested one).
fn contains_break(stmts: &[Node]) -> bool {
    for stmt in stmts {
        if stmt.kind() == "break_statement" {
            return true;
        }
        if LOOP_OR_SWITCH_TYPES.contains(&stmt.kind()) {
            continue;
        }
        if stmt.kind() == "compound_statement" {
            let mut cursor = stmt.walk();
            let children: Vec<Node> = stmt.named_children(&mut cursor).collect();
            if contains_break(&children) {
                return true;
            }
            continue;
        }
        for field in ["consequence", "body"] {
            if let Some(child) = stmt.child_by_field_name(field) {
                if contains_break(&block_stmts(Some(child))) {
                    return true;
                }
            }
        }
        if let Some(alternative) = stmt.child_by_field_name("alternative") {
            if let Some(inner) = alternative.named_child(0) {
                if contains_break(&block_stmts(Some(inner))) {
                    return true;
                }
            }
        }
    }
    false
}

/// Whether executing this statement (or block) always returns (never falls through).
fn always_exits(node: Option<Node>, source: &[u8]) -> bool {
    let stmts = block_stmts(node);
    match stmts.last() {
        Some(stmt) => stmt_always_exits(stmt, source),
        None => false,
    }
}

fn stmt_always_exits(stmt: &Node, source: &[u8]) -> bool {
    match stmt.kind() {
        "return_statement" => true,
        "if_statement" => {
            let Some(alternative) = stmt.child_by_field_name("alternative") else {
                return false;
            };
            let Some(inner) = alternative.named_child(0) else {
                return false;
            };
            always_exits(stmt.child_by_field_name("consequence"), source)
                && always_exits(Some(inner), source)
        }
        kind if INFINITE_LOOP_TYPES.contains(&kind) => {
            let is_infinite = stmt
                .child_by_field_name("condition")
                .and_then(|c| c.utf8_text(source).ok())
                .map(|text| matches!(text.trim_matches(|c: char| c == '(' || c == ')' || c == ' '), "1" | "true"))
                .unwrap_or(false);
            let body = block_stmts(stmt.child_by_field_name("body"));
            is_infinite && !contains_break(&body)
        }
        _ => false,
    }
}

impl Rule for MissingReturn {
    fn id(&self) -> &'static str {
        Self::RULE_ID
    }

    fn check(&self, tree: &Tree, source: &[u8], path: &str, _config: &Config) -> Vec<Diagnostic> {
        let mut diagnostics = Vec::new();
        let mut nodes = Vec::new();
        walk(tree.root_node(), &mut nodes);
        for func in nodes {
            if func.kind() != "function_definition" {
                continue;
            }
            if is_void_function(&func, source) {
                continue;
            }
            if !always_exits(func.child_by_field_name("body"), source) {
                let (line, col) = loc(&func);
                diagnostics.push(Diagnostic {
                    path: path.to_string(),
                    line,
                    col,
                    rule_id: Self::RULE_ID,
                    message: format!(
                        "Function `{}` may not return a value on all code paths",
                        function_name(&func, source)
                    ),
                });
            }
        }
        diagnostics
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn check(source: &str) -> Vec<Diagnostic> {
        let mut parser = tree_sitter::Parser::new();
        parser
            .set_language(&tree_sitter_c::LANGUAGE.into())
            .unwrap();
        let tree = parser.parse(source, None).unwrap();
        MissingReturn.check(&tree, source.as_bytes(), "example.c", &Config::default())
    }

    #[test]
    fn flags_missing_else_branch() {
        let diagnostics = check(
            "const char *classify(int x) {\n    if (x > 0) {\n        return \"positive\";\n    }\n}\n",
        );
        assert_eq!(diagnostics.len(), 1);
        assert_eq!(diagnostics[0].rule_id, "SA004");
    }

    #[test]
    fn ignores_complete_if_else() {
        let diagnostics = check(
            "const char *classify(int x) {\n    if (x > 0) {\n        return \"positive\";\n    } else {\n        return \"non-positive\";\n    }\n}\n",
        );
        assert_eq!(diagnostics, vec![]);
    }

    #[test]
    fn ignores_void_function() {
        let diagnostics = check(
            "void log_message(const char *message) {\n    printf(\"%s\", message);\n}\n",
        );
        assert_eq!(diagnostics, vec![]);
    }

    #[test]
    fn ignores_exhaustive_if_elif_else_chain() {
        let diagnostics = check(
            "int parse_value(const char *value) {\n    if (value == 0) {\n        return -1;\n    } else if (*value == '\\0') {\n        return 0;\n    } else {\n        return atoi(value);\n    }\n}\n",
        );
        assert_eq!(diagnostics, vec![]);
    }

    #[test]
    fn ignores_infinite_while_loop_without_break() {
        let diagnostics = check(
            "const char *serve(void) {\n    while (1) {\n        if (should_stop()) {\n            return \"done\";\n        }\n        handle();\n    }\n}\n",
        );
        assert_eq!(diagnostics, vec![]);
    }

    #[test]
    fn ignores_infinite_do_while_loop_without_break() {
        let diagnostics = check(
            "int serve_loop(void) {\n    do {\n        if (should_stop()) {\n            return 1;\n        }\n    } while (1);\n}\n",
        );
        assert_eq!(diagnostics, vec![]);
    }

    #[test]
    fn flags_while_loop_with_break() {
        let diagnostics = check(
            "int find_first(int n) {\n    while (1) {\n        if (n > 0) {\n            break;\n        }\n    }\n}\n",
        );
        assert_eq!(diagnostics.len(), 1);
    }
}
