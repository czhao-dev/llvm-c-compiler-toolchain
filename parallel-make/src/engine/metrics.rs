//! Lightweight runtime counters, useful for benchmarking and diagnostics.

use std::fmt;
use std::sync::atomic::{AtomicU64, Ordering};

#[derive(Default)]
pub(crate) struct Metrics {
    submitted: AtomicU64,
    completed: AtomicU64,
    panicked: AtomicU64,
    stolen: AtomicU64,
}

impl Metrics {
    pub(crate) fn record_submitted(&self) {
        self.submitted.fetch_add(1, Ordering::Relaxed);
    }

    pub(crate) fn record_completed(&self) {
        self.completed.fetch_add(1, Ordering::Relaxed);
    }

    pub(crate) fn record_panicked(&self) {
        self.panicked.fetch_add(1, Ordering::Relaxed);
    }

    pub(crate) fn record_stolen(&self) {
        self.stolen.fetch_add(1, Ordering::Relaxed);
    }

    pub(crate) fn snapshot(&self) -> RuntimeMetrics {
        RuntimeMetrics {
            tasks_submitted: self.submitted.load(Ordering::Relaxed),
            tasks_completed: self.completed.load(Ordering::Relaxed),
            tasks_panicked: self.panicked.load(Ordering::Relaxed),
            tasks_stolen: self.stolen.load(Ordering::Relaxed),
        }
    }
}

/// A point-in-time snapshot of runtime activity counters.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct RuntimeMetrics {
    /// Total tasks submitted via any `spawn*` call.
    pub tasks_submitted: u64,
    /// Total tasks that ran to completion (including panicking ones).
    pub tasks_completed: u64,
    /// Total tasks whose body panicked.
    pub tasks_panicked: u64,
    /// Total tasks that were picked up by a worker other than the one that
    /// pulled them off the global injector, i.e. true cross-worker steals.
    pub tasks_stolen: u64,
}

impl fmt::Display for RuntimeMetrics {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "submitted={} completed={} panicked={} stolen={}",
            self.tasks_submitted, self.tasks_completed, self.tasks_panicked, self.tasks_stolen
        )
    }
}
