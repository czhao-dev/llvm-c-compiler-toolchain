use parallel_make::makefile::{self, ParseError};

#[test]
fn parses_a_simple_rule() {
    let parsed = makefile::parse("foo: bar baz\n\techo building foo\n").unwrap();
    assert_eq!(parsed.rules.len(), 1);
    let rule = &parsed.rules[0];
    assert_eq!(rule.target, "foo");
    assert_eq!(rule.prereqs, vec!["bar".to_string(), "baz".to_string()]);
    assert_eq!(rule.recipe, vec!["echo building foo".to_string()]);
    assert!(!rule.phony);
}

#[test]
fn parses_multiple_recipe_lines() {
    let parsed = makefile::parse("foo:\n\techo one\n\techo two\n").unwrap();
    assert_eq!(parsed.rules[0].recipe, vec!["echo one", "echo two"]);
}

#[test]
fn target_with_no_prereqs() {
    let parsed = makefile::parse("foo:\n\techo hi\n").unwrap();
    assert!(parsed.rules[0].prereqs.is_empty());
}

#[test]
fn phony_declaration_marks_matching_rule() {
    let parsed = makefile::parse(".PHONY: clean\nclean:\n\trm -f out\n").unwrap();
    assert!(parsed.phony_targets.contains("clean"));
    let rule = parsed.rule_for("clean").unwrap();
    assert!(rule.phony);
}

#[test]
fn phony_can_precede_or_follow_the_rule() {
    let before = makefile::parse(".PHONY: clean\nclean:\n\trm -f out\n").unwrap();
    let after = makefile::parse("clean:\n\trm -f out\n.PHONY: clean\n").unwrap();
    assert!(before.rule_for("clean").unwrap().phony);
    assert!(after.rule_for("clean").unwrap().phony);
}

#[test]
fn comments_and_blank_lines_are_skipped() {
    let parsed = makefile::parse(
        "# this is a comment\n\nfoo: bar # inline comment\n\techo hi\n\n# trailing\n",
    )
    .unwrap();
    assert_eq!(parsed.rules.len(), 1);
    assert_eq!(parsed.rules[0].prereqs, vec!["bar".to_string()]);
}

#[test]
fn default_goal_is_first_non_dot_target() {
    let parsed = makefile::parse(".PHONY: all\nall: a b\na:\n\techo a\nb:\n\techo b\n").unwrap();
    assert_eq!(parsed.default_goal(), Some("all"));
}

#[test]
fn recipe_line_without_a_preceding_rule_is_an_error() {
    let err = makefile::parse("\techo orphan\n").unwrap_err();
    assert_eq!(err, ParseError::OrphanRecipeLine(1));
}

#[test]
fn header_without_colon_is_an_error() {
    let err = makefile::parse("this has no colon\n").unwrap_err();
    assert_eq!(err, ParseError::MalformedHeader(1));
}

#[test]
fn discover_finds_a_makefile() {
    // Written lowercase; on case-insensitive filesystems (e.g. default
    // macOS) this also satisfies the "Makefile" lookup, so we only assert
    // discovery succeeds, not which exact casing matched.
    let dir = std::env::temp_dir().join(format!(
        "parallel-make-parser-test-{}-{}",
        std::process::id(),
        line!()
    ));
    std::fs::create_dir_all(&dir).unwrap();
    std::fs::write(dir.join("makefile"), "foo:\n\techo hi\n").unwrap();

    let found = makefile::discover(&dir).unwrap();
    assert_eq!(found.file_name().unwrap().to_ascii_lowercase(), "makefile");

    std::fs::remove_dir_all(&dir).unwrap();
}

#[test]
fn discover_returns_none_when_absent() {
    let dir = std::env::temp_dir().join(format!(
        "parallel-make-parser-test-empty-{}-{}",
        std::process::id(),
        line!()
    ));
    std::fs::create_dir_all(&dir).unwrap();

    assert!(makefile::discover(&dir).is_none());

    std::fs::remove_dir_all(&dir).unwrap();
}
