//! Minimal Makefile parsing: explicit rules, `.PHONY` declarations,
//! comments, and tab-indented recipe lines.
//!
//! Variable expansion, automatic variables, and pattern rules are not
//! supported in this mini implementation — see the README for the full
//! list of deferred features.

use std::collections::HashSet;
use std::fmt;
use std::path::{Path, PathBuf};

/// A single explicit rule: one target, its prerequisites, and the shell
/// commands that rebuild it.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Rule {
    pub target: String,
    pub prereqs: Vec<String>,
    pub recipe: Vec<String>,
    pub phony: bool,
}

/// The result of parsing a Makefile: its rules in source order (used to
/// pick the default goal) plus the set of names declared `.PHONY`.
#[derive(Debug, Clone, Default)]
pub struct ParsedMakefile {
    pub rules: Vec<Rule>,
    pub phony_targets: HashSet<String>,
}

impl ParsedMakefile {
    /// The first rule whose target doesn't start with `.`, used as the
    /// default goal when none is given on the command line.
    pub fn default_goal(&self) -> Option<&str> {
        self.rules
            .iter()
            .find(|r| !r.target.starts_with('.'))
            .map(|r| r.target.as_str())
    }

    /// Looks up the rule for a given target name, if one exists.
    pub fn rule_for(&self, target: &str) -> Option<&Rule> {
        self.rules.iter().find(|r| r.target == target)
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum ParseError {
    /// A tab-indented recipe line appeared with no preceding rule header.
    OrphanRecipeLine(usize),
    /// A rule header line had no `:`.
    MalformedHeader(usize),
}

impl fmt::Display for ParseError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ParseError::OrphanRecipeLine(line) => {
                write!(f, "{line}: recipe line has no target")
            }
            ParseError::MalformedHeader(line) => {
                write!(f, "{line}: missing separator ':'")
            }
        }
    }
}

impl std::error::Error for ParseError {}

/// Parses Makefile text into rules and `.PHONY` declarations.
///
/// Lines are processed as follows:
/// - Blank lines and `#`-comment lines are skipped.
/// - A line starting with a tab is a recipe line, attached to the most
///   recently started rule header.
/// - A line of the form `.PHONY: a b c` records `a`, `b`, `c` as phony
///   target names (applied to matching rules after the full parse).
/// - Any other non-blank line is a rule header `target: prereq prereq...`.
pub fn parse(text: &str) -> Result<ParsedMakefile, ParseError> {
    let mut rules: Vec<Rule> = Vec::new();
    let mut phony_targets: HashSet<String> = HashSet::new();
    let mut current: Option<usize> = None;

    for (idx, raw_line) in text.lines().enumerate() {
        let line_no = idx + 1;

        if let Some(recipe_line) = raw_line.strip_prefix('\t') {
            let Some(current_idx) = current else {
                return Err(ParseError::OrphanRecipeLine(line_no));
            };
            rules[current_idx].recipe.push(recipe_line.to_string());
            continue;
        }

        let without_comment = strip_comment(raw_line);
        let trimmed = without_comment.trim();
        if trimmed.is_empty() {
            continue;
        }

        let Some(colon_idx) = trimmed.find(':') else {
            return Err(ParseError::MalformedHeader(line_no));
        };
        let head = trimmed[..colon_idx].trim();
        let rest = trimmed[colon_idx + 1..].trim();
        let names: Vec<String> = rest.split_whitespace().map(str::to_string).collect();

        if head == ".PHONY" {
            phony_targets.extend(names);
            current = None;
            continue;
        }

        current = Some(rules.len());
        rules.push(Rule {
            target: head.to_string(),
            prereqs: names,
            recipe: Vec::new(),
            phony: false,
        });
    }

    for rule in &mut rules {
        if phony_targets.contains(&rule.target) {
            rule.phony = true;
        }
    }

    Ok(ParsedMakefile {
        rules,
        phony_targets,
    })
}

fn strip_comment(line: &str) -> &str {
    match line.find('#') {
        Some(idx) => &line[..idx],
        None => line,
    }
}

/// Looks for `Makefile` then `makefile` inside `dir`, returning the first
/// one found.
pub fn discover(dir: &Path) -> Option<PathBuf> {
    for name in ["Makefile", "makefile"] {
        let candidate = dir.join(name);
        if candidate.is_file() {
            return Some(candidate);
        }
    }
    None
}
