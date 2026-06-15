//! Port of Python's `fnmatch.fnmatch` glob semantics: `*`, `?`, `[seq]`, `[!seq]`.
//! POSIX `os.path.normcase` is a no-op, so matching stays case-sensitive.

enum Atom {
    Lit(char),
    Any,
    Star,
    Set(bool, Vec<(char, char)>),
}

fn parse_pattern(pattern: &str) -> Vec<Atom> {
    let chars: Vec<char> = pattern.chars().collect();
    let mut atoms = Vec::new();
    let mut i = 0;
    while i < chars.len() {
        match chars[i] {
            '*' => {
                atoms.push(Atom::Star);
                i += 1;
            }
            '?' => {
                atoms.push(Atom::Any);
                i += 1;
            }
            '[' => {
                let mut j = i + 1;
                let mut negate = false;
                if j < chars.len() && chars[j] == '!' {
                    negate = true;
                    j += 1;
                }
                let set_start = j;
                if j < chars.len() && chars[j] == ']' {
                    j += 1;
                }
                while j < chars.len() && chars[j] != ']' {
                    j += 1;
                }
                if j >= chars.len() {
                    atoms.push(Atom::Lit('['));
                    i += 1;
                } else {
                    let set_chars = &chars[set_start..j];
                    let mut ranges = Vec::new();
                    let mut k = 0;
                    while k < set_chars.len() {
                        if k + 2 < set_chars.len() && set_chars[k + 1] == '-' {
                            ranges.push((set_chars[k], set_chars[k + 2]));
                            k += 3;
                        } else {
                            ranges.push((set_chars[k], set_chars[k]));
                            k += 1;
                        }
                    }
                    atoms.push(Atom::Set(negate, ranges));
                    i = j + 1;
                }
            }
            c => {
                atoms.push(Atom::Lit(c));
                i += 1;
            }
        }
    }
    atoms
}

fn matches(name: &[char], pat: &[Atom]) -> bool {
    match pat.first() {
        None => name.is_empty(),
        Some(Atom::Star) => (0..=name.len()).any(|i| matches(&name[i..], &pat[1..])),
        Some(Atom::Any) => !name.is_empty() && matches(&name[1..], &pat[1..]),
        Some(Atom::Lit(c)) => name.first() == Some(c) && matches(&name[1..], &pat[1..]),
        Some(Atom::Set(negate, ranges)) => match name.first() {
            Some(&ch) => {
                let in_set = ranges.iter().any(|&(lo, hi)| ch >= lo && ch <= hi);
                in_set != *negate && matches(&name[1..], &pat[1..])
            }
            None => false,
        },
    }
}

pub fn fnmatch(name: &str, pattern: &str) -> bool {
    let name_chars: Vec<char> = name.chars().collect();
    let pattern_atoms = parse_pattern(pattern);
    matches(&name_chars, &pattern_atoms)
}

#[cfg(test)]
mod tests {
    use super::fnmatch;

    #[test]
    fn star_matches_any_suffix() {
        assert!(fnmatch("tests/test_foo.py", "tests/*"));
        assert!(fnmatch("foo.py", "*.py"));
        assert!(!fnmatch("foo.txt", "*.py"));
    }

    #[test]
    fn question_mark_matches_single_char() {
        assert!(fnmatch("a.py", "?.py"));
        assert!(!fnmatch("ab.py", "?.py"));
    }

    #[test]
    fn bracket_set_matches_listed_chars() {
        assert!(fnmatch("a.py", "[abc].py"));
        assert!(!fnmatch("d.py", "[abc].py"));
        assert!(fnmatch("d.py", "[!abc].py"));
    }

    #[test]
    fn bracket_range_matches() {
        assert!(fnmatch("m.py", "[a-z].py"));
        assert!(!fnmatch("M.py", "[a-z].py"));
    }
}
