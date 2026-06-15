use tree_sitter::{Node, Tree};

use super::Rule;
use crate::config::Config;
use crate::diagnostics::Diagnostic;
use crate::visitor::{loc, walk};

pub struct UnreachableCode;

impl UnreachableCode {
    pub const RULE_ID: &'static str = "SA005";
}

fn keyword_for(stmt: &Node) -> Option<&'static str> {
    match stmt.kind() {
        "return_statement" => Some("return"),
        "break_statement" => Some("break"),
        "continue_statement" => Some("continue"),
        "goto_statement" => Some("goto"),
        _ => None,
    }
}

fn case_body_stmts<'a>(case_stmt: &Node<'a>) -> Vec<Node<'a>> {
    let value = case_stmt.child_by_field_name("value");
    let mut cursor = case_stmt.walk();
    case_stmt
        .named_children(&mut cursor)
        .filter(|c| Some(*c) != value)
        .collect()
}

fn check_block(stmts: &[Node], path: &str) -> Option<Diagnostic> {
    let last = stmts.len().saturating_sub(1);
    for (i, stmt) in stmts.iter().take(last).enumerate() {
        if let Some(keyword) = keyword_for(stmt) {
            let unreachable = &stmts[i + 1];
            let (line, col) = loc(unreachable);
            return Some(Diagnostic {
                path: path.to_string(),
                line,
                col,
                rule_id: UnreachableCode::RULE_ID,
                message: format!("Unreachable code after `{keyword}`"),
            });
        }
    }
    None
}

impl Rule for UnreachableCode {
    fn id(&self) -> &'static str {
        Self::RULE_ID
    }

    fn check(&self, tree: &Tree, _source: &[u8], path: &str, _config: &Config) -> Vec<Diagnostic> {
        let mut diagnostics = Vec::new();
        let mut nodes = Vec::new();
        walk(tree.root_node(), &mut nodes);
        for node in nodes {
            if node.kind() == "compound_statement" {
                let mut cursor = node.walk();
                let stmts: Vec<Node> = node.named_children(&mut cursor).collect();
                if let Some(diagnostic) = check_block(&stmts, path) {
                    diagnostics.push(diagnostic);
                }
            } else if node.kind() == "case_statement" {
                let stmts = case_body_stmts(&node);
                if let Some(diagnostic) = check_block(&stmts, path) {
                    diagnostics.push(diagnostic);
                }
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
        UnreachableCode.check(&tree, source.as_bytes(), "example.c", &Config::default())
    }

    #[test]
    fn flags_code_after_return() {
        let diagnostics =
            check("int f(void) {\n    return 1;\n    printf(\"never runs\");\n}\n");
        assert_eq!(diagnostics.len(), 1);
        assert_eq!(diagnostics[0].rule_id, "SA005");
        assert_eq!(diagnostics[0].line, 3);
    }

    #[test]
    fn flags_code_after_break_in_loop() {
        let diagnostics = check(
            "void f(void) {\n    for (int i = 0; i < 10; i++) {\n        break;\n        printf(\"%d\", i);\n    }\n}\n",
        );
        assert_eq!(diagnostics.len(), 1);
    }

    #[test]
    fn flags_code_after_return_in_case() {
        let diagnostics = check(
            "int f(int x) {\n    switch (x) {\n        case 1:\n            return 1;\n            return 2;\n    }\n    return 0;\n}\n",
        );
        assert_eq!(diagnostics.len(), 1);
    }

    #[test]
    fn ignores_reachable_code() {
        let diagnostics = check("int f(int x) {\n    if (x) {\n        return 1;\n    }\n    return 2;\n}\n");
        assert_eq!(diagnostics, vec![]);
    }
}
