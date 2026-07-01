//! Task priority classes.

/// Scheduling priority for a task.
///
/// Workers always prefer `High` work over `Normal` work over `Background`
/// work, both in their own local queue and when pulling from the global
/// injector or stealing from a peer.
#[repr(usize)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Default)]
pub enum Priority {
    High = 0,
    #[default]
    Normal = 1,
    Background = 2,
}

pub(crate) const PRIORITY_COUNT: usize = 3;

/// Polling order workers use when looking for the next task.
pub(crate) const PRIORITY_ORDER: [Priority; PRIORITY_COUNT] =
    [Priority::High, Priority::Normal, Priority::Background];
