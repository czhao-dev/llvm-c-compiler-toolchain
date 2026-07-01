use std::fmt;

/// Field order is load-bearing: `Ord` compares fields top-to-bottom, mirroring
/// the Python dataclass's `order=True` (path, line, col, rule_id, message).
#[derive(Debug, Clone, PartialEq, Eq, PartialOrd, Ord)]
pub struct Diagnostic {
    pub path: String,
    pub line: usize,
    pub col: usize,
    pub rule_id: &'static str,
    pub message: String,
}

impl fmt::Display for Diagnostic {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "{}:{}: {} {}",
            self.path, self.line, self.rule_id, self.message
        )
    }
}
