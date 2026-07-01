use tree_sitter::{Node, Tree};

use super::Rule;
use crate::config::Config;
use crate::diagnostics::Diagnostic;
use crate::visitor::{loc, walk};

pub struct Nesting;

impl Nesting {
    pub const RULE_ID: &'static str = "SA003";
}

const LOOP_TYPES: &[&str] = &["for_statement", "while_statement", "do_statement"];

fn case_body_stmts<'a>(case_stmt: &Node<'a>) -> Vec<Node<'a>> {
    let value = case_stmt.child_by_field_name("value");
    let mut cursor = case_stmt.walk();
    case_stmt
        .named_children(&mut cursor)
        .filter(|c| Some(*c) != value)
        .collect()
}

struct Scanner<'a> {
    path: &'a str,
    threshold: i64,
    diagnostics: Vec<Diagnostic>,
}

impl<'a> Scanner<'a> {
    fn maybe_report(&mut self, node: &Node, depth: i64, reported: &mut bool) {
        if depth > self.threshold && !*reported {
            *reported = true;
            let (line, col) = loc(node);
            self.diagnostics.push(Diagnostic {
                path: self.path.to_string(),
                line,
                col,
                rule_id: Nesting::RULE_ID,
                message: format!(
                    "Control flow nested {depth} levels deep (threshold {})",
                    self.threshold
                ),
            });
        }
    }

    fn walk_stmt_or_block(&mut self, stmt: Option<Node>, depth: i64, reported: &mut bool) {
        let Some(stmt) = stmt else {
            return;
        };
        if stmt.kind() == "compound_statement" {
            let mut cursor = stmt.walk();
            for child in stmt.named_children(&mut cursor).collect::<Vec<_>>() {
                self.walk_stmt(child, depth, reported);
            }
        } else {
            self.walk_stmt(stmt, depth, reported);
        }
    }

    fn walk_if(&mut self, stmt: Node, depth: i64, reported: &mut bool) {
        let new_depth = depth + 1;
        self.maybe_report(&stmt, new_depth, reported);
        self.walk_stmt_or_block(stmt.child_by_field_name("consequence"), new_depth, reported);

        let Some(alternative) = stmt.child_by_field_name("alternative") else {
            return;
        };
        let Some(inner) = alternative.named_child(0) else {
            return;
        };
        if inner.kind() == "if_statement" {
            self.walk_if(inner, depth, reported); // elif chains don't add a nesting level
        } else {
            self.walk_stmt_or_block(Some(inner), new_depth, reported);
        }
    }

    fn walk_stmt(&mut self, stmt: Node, depth: i64, reported: &mut bool) {
        if stmt.kind() == "if_statement" {
            self.walk_if(stmt, depth, reported);
        } else if LOOP_TYPES.contains(&stmt.kind()) {
            let new_depth = depth + 1;
            self.maybe_report(&stmt, new_depth, reported);
            self.walk_stmt_or_block(stmt.child_by_field_name("body"), new_depth, reported);
        } else if stmt.kind() == "switch_statement" {
            let new_depth = depth + 1;
            self.maybe_report(&stmt, new_depth, reported);
            let Some(body) = stmt.child_by_field_name("body") else {
                return;
            };
            let mut cursor = body.walk();
            for case_stmt in body.named_children(&mut cursor).collect::<Vec<_>>() {
                if case_stmt.kind() != "case_statement" {
                    continue;
                }
                for sub in case_body_stmts(&case_stmt) {
                    self.walk_stmt(sub, new_depth, reported);
                }
            }
        }
    }
}

impl Rule for Nesting {
    fn id(&self) -> &'static str {
        Self::RULE_ID
    }

    fn check(&self, tree: &Tree, _source: &[u8], path: &str, config: &Config) -> Vec<Diagnostic> {
        let mut scanner = Scanner {
            path,
            threshold: config.max_nesting,
            diagnostics: Vec::new(),
        };
        let mut nodes = Vec::new();
        walk(tree.root_node(), &mut nodes);
        for func in nodes {
            if func.kind() != "function_definition" {
                continue;
            }
            let Some(body) = func.child_by_field_name("body") else {
                continue;
            };
            let mut reported = false;
            let mut cursor = body.walk();
            for stmt in body.named_children(&mut cursor).collect::<Vec<_>>() {
                scanner.walk_stmt(stmt, 0, &mut reported);
            }
        }
        scanner.diagnostics
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
        Nesting.check(&tree, source.as_bytes(), "example.c", config)
    }

    #[test]
    fn shallow_nesting_is_not_flagged() {
        let diagnostics = check(
            "int f(int x) {\n    if (x) {\n        return 1;\n    }\n    return 0;\n}\n",
            &Config::default(),
        );
        assert_eq!(diagnostics, vec![]);
    }

    #[test]
    fn flags_deep_nesting() {
        let config = Config {
            max_nesting: 2,
            ..Config::default()
        };
        let source = "int f(int x) {\n    if (x) {\n        for (int i = 0; i < x; i++) {\n            while (i > 0) {\n                i--;\n            }\n        }\n    }\n    return 0;\n}\n";
        let diagnostics = check(source, &config);
        assert_eq!(diagnostics.len(), 1);
        assert_eq!(diagnostics[0].rule_id, "SA003");
    }

    #[test]
    fn elif_chain_does_not_count_as_nesting() {
        let config = Config {
            max_nesting: 1,
            ..Config::default()
        };
        let source = "const char *f(int x) {\n    if (x == 1) {\n        return \"one\";\n    } else if (x == 2) {\n        return \"two\";\n    } else if (x == 3) {\n        return \"three\";\n    } else {\n        return \"other\";\n    }\n}\n";
        let diagnostics = check(source, &config);
        assert_eq!(diagnostics, vec![]);
    }

    #[test]
    fn else_with_nested_if_does_count() {
        let config = Config {
            max_nesting: 1,
            ..Config::default()
        };
        let source = "int f(int x, int y) {\n    if (x) {\n        return 1;\n    } else {\n        if (y) {\n            return 2;\n        }\n    }\n}\n";
        let diagnostics = check(source, &config);
        assert_eq!(diagnostics.len(), 1);
    }

    #[test]
    fn reports_only_once_per_function() {
        let config = Config {
            max_nesting: 1,
            ..Config::default()
        };
        let source = "int f(int x) {\n    if (x) {\n        if (x) {\n            if (x) {\n                return 1;\n            }\n        }\n    }\n    return 0;\n}\n";
        let diagnostics = check(source, &config);
        assert_eq!(diagnostics.len(), 1);
    }

    #[test]
    fn switch_case_counts_as_nesting() {
        let config = Config {
            max_nesting: 1,
            ..Config::default()
        };
        let source = "int f(int x) {\n    switch (x) {\n        case 1:\n            if (x) {\n                return 1;\n            }\n            return 0;\n    }\n    return 0;\n}\n";
        let diagnostics = check(source, &config);
        assert_eq!(diagnostics.len(), 1);
    }
}
