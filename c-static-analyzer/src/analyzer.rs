use std::path::{Path, PathBuf};

use crate::config::Config;
use crate::diagnostics::Diagnostic;
use crate::fnmatch::fnmatch;
use crate::rules::ALL_RULES;

pub const DEFAULT_EXCLUDE_DIRS: &[&str] = &[
    ".git",
    "build",
    "dist",
    "cmake-build-debug",
    "cmake-build-release",
    "CMakeFiles",
    "out",
    "vendor",
    "third_party",
];

const EXTENSIONS: &[&str] = &["c", "h"];

pub fn is_excluded(path: &Path, patterns: &[String]) -> bool {
    let in_default_excluded_dir = path.components().any(|component| {
        let std::path::Component::Normal(part) = component else {
            return false;
        };
        DEFAULT_EXCLUDE_DIRS.contains(&part.to_string_lossy().as_ref())
    });
    if in_default_excluded_dir {
        return true;
    }
    let posix = path.to_string_lossy().replace('\\', "/");
    let filename = path
        .file_name()
        .map(|f| f.to_string_lossy().to_string())
        .unwrap_or_default();
    patterns
        .iter()
        .any(|pattern| fnmatch(&posix, pattern) || fnmatch(&filename, pattern))
}

fn collect_c_files(dir: &Path, out: &mut Vec<PathBuf>) {
    let Ok(entries) = std::fs::read_dir(dir) else {
        return;
    };
    for entry in entries.flatten() {
        let path = entry.path();
        if path.is_dir() {
            collect_c_files(&path, out);
        } else if path
            .extension()
            .is_some_and(|ext| EXTENSIONS.iter().any(|e| ext == *e))
        {
            out.push(path);
        }
    }
}

pub fn iter_c_files(paths: &[PathBuf], exclude: &[String]) -> Vec<PathBuf> {
    let mut out = Vec::new();
    for path in paths {
        if path.is_file() {
            if path
                .extension()
                .is_some_and(|ext| EXTENSIONS.iter().any(|e| ext == *e))
                && !is_excluded(path, exclude)
            {
                out.push(path.clone());
            }
            continue;
        }
        let mut found = Vec::new();
        collect_c_files(path, &mut found);
        found.sort();
        for candidate in found {
            if !is_excluded(&candidate, exclude) {
                out.push(candidate);
            }
        }
    }
    out
}

pub fn analyze_file(path: &Path, config: &Config) -> Vec<Diagnostic> {
    let path_str = path.to_string_lossy().to_string();

    let source = match std::fs::read(path) {
        Ok(source) => source,
        Err(err) => {
            return vec![Diagnostic {
                path: path_str,
                line: 1,
                col: 0,
                rule_id: "SA000",
                message: format!("Could not read file: {err}"),
            }];
        }
    };

    let mut parser = tree_sitter::Parser::new();
    parser
        .set_language(&tree_sitter_c::LANGUAGE.into())
        .expect("the C grammar must load");
    let tree = parser.parse(&source, None).expect("parsing never fails");

    let mut diagnostics = Vec::new();
    for rule in ALL_RULES {
        if !config.is_enabled(rule.id()) {
            continue;
        }
        diagnostics.extend(rule.check(&tree, &source, &path_str, config));
    }
    diagnostics
}

pub fn analyze_paths(paths: &[PathBuf], config: &Config) -> Vec<Diagnostic> {
    let mut diagnostics = Vec::new();
    for file_path in iter_c_files(paths, &config.exclude) {
        diagnostics.extend(analyze_file(&file_path, config));
    }
    diagnostics.sort();
    diagnostics
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn excludes_default_dirs() {
        let path = PathBuf::from("project/build/obj/foo.c");
        assert!(is_excluded(&path, &[]));
    }

    #[test]
    fn excludes_user_glob_patterns() {
        // fnmatch is a full (anchored) match, so the pattern must match the
        // entire posix path or the bare filename, not just a substring.
        let path = PathBuf::from("tests/test_foo.c");
        assert!(is_excluded(&path, &["tests/*".to_string()]));
        assert!(!is_excluded(&path, &["other/*".to_string()]));
        assert!(is_excluded(&path, &["test_*.c".to_string()]));
    }

    #[test]
    fn iter_c_files_finds_c_and_h_files_sorted() {
        let dir = tempfile::tempdir().unwrap();
        std::fs::write(dir.path().join("b.c"), "").unwrap();
        std::fs::write(dir.path().join("a.h"), "").unwrap();
        std::fs::write(dir.path().join("c.txt"), "").unwrap();
        let found = iter_c_files(&[dir.path().to_path_buf()], &[]);
        assert_eq!(
            found,
            vec![dir.path().join("a.h"), dir.path().join("b.c")]
        );
    }
}
