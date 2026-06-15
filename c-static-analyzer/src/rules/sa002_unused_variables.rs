use std::collections::{HashMap, HashSet};

use tree_sitter::{Node, Tree};

use super::Rule;
use crate::config::Config;
use crate::diagnostics::Diagnostic;
use crate::visitor::{loc, walk};

pub struct UnusedVariables;

impl UnusedVariables {
    pub const RULE_ID: &'static str = "SA002";
}

fn base_identifier<'a>(declarator: Node<'a>) -> Option<Node<'a>> {
    let mut current = declarator;
    while current.kind() != "identifier" {
        current = current.child_by_field_name("declarator")?;
    }
    Some(current)
}

/// First declaration site per local variable name, keyed by name.
fn declared_names<'a>(body: Node<'a>, source: &[u8]) -> HashMap<String, Node<'a>> {
    let mut declared = HashMap::new();
    let mut nodes = Vec::new();
    walk(body, &mut nodes);
    for node in nodes {
        if node.kind() != "declaration" {
            continue;
        }
        let mut cursor = node.walk();
        for declarator in node.children_by_field_name("declarator", &mut cursor) {
            let Some(name_node) = base_identifier(declarator) else {
                continue;
            };
            let Ok(name) = name_node.utf8_text(source) else {
                continue;
            };
            if name.starts_with('_') {
                continue;
            }
            declared.entry(name.to_string()).or_insert(name_node);
        }
    }
    declared
}

fn is_plain_assignment_target(node: &Node, source: &[u8]) -> bool {
    let Some(parent) = node.parent() else {
        return false;
    };
    if parent.kind() != "assignment_expression" {
        return false;
    }
    let left = parent.child_by_field_name("left");
    let operator = parent.child_by_field_name("operator");
    left.is_some_and(|n| n == *node) && operator.is_some_and(|op| op.utf8_text(source) == Ok("="))
}

fn collect_used(body: Node, source: &[u8], declared_site_ids: &HashSet<usize>) -> HashSet<String> {
    let mut used = HashSet::new();
    let mut nodes = Vec::new();
    walk(body, &mut nodes);
    for node in nodes {
        if node.kind() != "identifier" {
            continue;
        }
        if declared_site_ids.contains(&node.id()) || is_plain_assignment_target(&node, source) {
            continue;
        }
        if let Ok(text) = node.utf8_text(source) {
            used.insert(text.to_string());
        }
    }
    used
}

impl Rule for UnusedVariables {
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
            let Some(body) = func.child_by_field_name("body") else {
                continue;
            };

            let declared = declared_names(body, source);
            let declared_site_ids: HashSet<usize> = declared.values().map(|n| n.id()).collect();
            let used = collect_used(body, source, &declared_site_ids);

            for (name, name_node) in &declared {
                if used.contains(name) {
                    continue;
                }
                let (line, col) = loc(name_node);
                diagnostics.push(Diagnostic {
                    path: path.to_string(),
                    line,
                    col,
                    rule_id: Self::RULE_ID,
                    message: format!("Local variable `{name}` is assigned but never used"),
                });
            }
        }
        diagnostics.sort();
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
        UnusedVariables.check(&tree, source.as_bytes(), "example.c", &Config::default())
    }

    #[test]
    fn flags_unused_local_variable() {
        let diagnostics = check(
            "int compute(void) {\n    int total = 0;\n    int unused = 42;\n    return total;\n}\n",
        );
        assert_eq!(diagnostics.len(), 1);
        assert!(diagnostics[0].message.contains("unused"));
        assert_eq!(diagnostics[0].rule_id, "SA002");
    }

    #[test]
    fn ignores_used_variable() {
        let diagnostics = check(
            "int compute(void) {\n    int total = 0;\n    for (int i = 0; i < 10; i++) {\n        total += i;\n    }\n    return total;\n}\n",
        );
        assert_eq!(diagnostics, vec![]);
    }

    #[test]
    fn ignores_underscore_prefixed() {
        let diagnostics =
            check("int compute(void) {\n    int _ignored = expensive_call();\n    return 1;\n}\n");
        assert_eq!(diagnostics, vec![]);
    }

    #[test]
    fn ignores_global_variable_mutation() {
        let diagnostics = check(
            "int counter = 0;\n\nvoid increment(void) {\n    counter = counter + 1;\n}\n",
        );
        assert_eq!(diagnostics, vec![]);
    }

    #[test]
    fn array_size_and_initializer_count_as_use() {
        let diagnostics = check(
            "int compute(int n) {\n    int size = n;\n    int values[size];\n    return values[0];\n}\n",
        );
        assert_eq!(diagnostics, vec![]);
    }
}
