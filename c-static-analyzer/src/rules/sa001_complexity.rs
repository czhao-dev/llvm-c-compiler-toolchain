use tree_sitter::{Node, Tree};

use super::Rule;
use crate::config::Config;
use crate::diagnostics::Diagnostic;
use crate::visitor::{function_name, loc, walk};

pub struct Complexity;

impl Complexity {
    pub const RULE_ID: &'static str = "SA001";
}

const BRANCH_TYPES: &[&str] = &[
    "if_statement",
    "for_statement",
    "while_statement",
    "do_statement",
    "conditional_expression",
];
const BOOL_OPERATORS: &[&str] = &["&&", "||"];

fn score(node: &Node, source: &[u8]) -> i64 {
    let mut total = 0;
    if BRANCH_TYPES.contains(&node.kind()) {
        total += 1;
    } else if node.kind() == "case_statement" && node.child_by_field_name("value").is_some() {
        total += 1;
    } else if node.kind() == "binary_expression" {
        if let Some(operator) = node.child_by_field_name("operator") {
            if let Ok(text) = operator.utf8_text(source) {
                if BOOL_OPERATORS.contains(&text) {
                    total += 1;
                }
            }
        }
    }
    let mut cursor = node.walk();
    for child in node.children(&mut cursor) {
        total += score(&child, source);
    }
    total
}

fn compute_complexity(func: &Node, source: &[u8]) -> i64 {
    let mut complexity = 1;
    let mut cursor = func.walk();
    for child in func.children(&mut cursor) {
        complexity += score(&child, source);
    }
    complexity
}

impl Rule for Complexity {
    fn id(&self) -> &'static str {
        Self::RULE_ID
    }

    fn check(&self, tree: &Tree, source: &[u8], path: &str, config: &Config) -> Vec<Diagnostic> {
        let mut diagnostics = Vec::new();
        let threshold = config.max_complexity;
        let mut nodes = Vec::new();
        walk(tree.root_node(), &mut nodes);
        for node in &nodes {
            if node.kind() != "function_definition" {
                continue;
            }
            let score = compute_complexity(node, source);
            if score > threshold {
                let (line, col) = loc(node);
                diagnostics.push(Diagnostic {
                    path: path.to_string(),
                    line,
                    col,
                    rule_id: Self::RULE_ID,
                    message: format!(
                        "Function `{}` has cyclomatic complexity {} (threshold {})",
                        function_name(node, source),
                        score,
                        threshold
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

    fn check(source: &str, config: &Config) -> Vec<Diagnostic> {
        let mut parser = tree_sitter::Parser::new();
        parser
            .set_language(&tree_sitter_c::LANGUAGE.into())
            .unwrap();
        let tree = parser.parse(source, None).unwrap();
        Complexity.check(&tree, source.as_bytes(), "example.c", config)
    }

    #[test]
    fn simple_function_is_not_flagged() {
        let diagnostics = check(
            "int add(int a, int b) {\n    return a + b;\n}\n",
            &Config::default(),
        );
        assert_eq!(diagnostics, vec![]);
    }

    #[test]
    fn flags_high_complexity_function() {
        let config = Config {
            max_complexity: 3,
            ..Config::default()
        };
        let source = "const char *classify(int x) {\n    if (x > 0) {\n        if (x > 10) {\n            return \"big\";\n        }\n        return \"small\";\n    } else if (x < 0) {\n        return \"negative\";\n    }\n    return \"zero\";\n}\n";
        let diagnostics = check(source, &config);
        assert_eq!(diagnostics.len(), 1);
        assert_eq!(diagnostics[0].rule_id, "SA001");
    }

    #[test]
    fn multiple_functions_scored_independently() {
        let config = Config {
            max_complexity: 1,
            ..Config::default()
        };
        let source = "int outer(void) {\n    return 1;\n}\n\nint inner(int x) {\n    if (x) {\n        return 1;\n    }\n    return 2;\n}\n";
        let diagnostics = check(source, &config);
        let names: std::collections::HashSet<&str> = diagnostics
            .iter()
            .map(|d| d.message.split('`').nth(1).unwrap())
            .collect();
        assert_eq!(names, std::collections::HashSet::from(["inner"]));
    }
}
