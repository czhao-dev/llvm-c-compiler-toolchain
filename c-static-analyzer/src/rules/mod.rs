use crate::config::Config;
use crate::diagnostics::Diagnostic;

pub trait Rule {
    fn id(&self) -> &'static str;
    fn check(&self, tree: &tree_sitter::Tree, source: &[u8], path: &str, config: &Config) -> Vec<Diagnostic>;
}

mod sa001_complexity;
mod sa002_unused_variables;
mod sa003_nesting;
mod sa004_missing_return;
mod sa005_unreachable_code;

pub use sa001_complexity::Complexity;
pub use sa002_unused_variables::UnusedVariables;
pub use sa003_nesting::Nesting;
pub use sa004_missing_return::MissingReturn;
pub use sa005_unreachable_code::UnreachableCode;

pub const ALL_RULES: &[&dyn Rule] = &[
    &Complexity,
    &UnusedVariables,
    &Nesting,
    &MissingReturn,
    &UnreachableCode,
];
