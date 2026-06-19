//! Cooperative cancellation primitives.
//!
//! The runtime never forcibly kills a worker thread mid-task, since that
//! would be unsound in the presence of locks and destructors. Instead a
//! [`CancellationToken`] is a shared flag that a running task can poll via
//! [`CancellationContext::is_cancelled`] and exit early on its own terms.

use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;

/// A cloneable, shareable cancellation flag.
///
/// Cloning a token does not create a new flag; all clones observe the same
/// cancellation state.
#[derive(Clone, Default)]
pub struct CancellationToken {
    flag: Arc<AtomicBool>,
}

impl CancellationToken {
    /// Creates a new, not-yet-cancelled token.
    pub fn new() -> Self {
        Self {
            flag: Arc::new(AtomicBool::new(false)),
        }
    }

    /// Requests cancellation. Idempotent.
    pub fn cancel(&self) {
        self.flag.store(true, Ordering::SeqCst);
    }

    /// Returns `true` if [`cancel`](Self::cancel) has been called.
    pub fn is_cancelled(&self) -> bool {
        self.flag.load(Ordering::SeqCst)
    }

    pub(crate) fn context(&self) -> CancellationContext {
        CancellationContext {
            token: self.clone(),
        }
    }
}

/// Passed into a task spawned via `spawn_cancellable`, letting the task body
/// poll for cancellation requests.
pub struct CancellationContext {
    token: CancellationToken,
}

impl CancellationContext {
    /// Returns `true` if the associated token has been cancelled.
    pub fn is_cancelled(&self) -> bool {
        self.token.is_cancelled()
    }
}
