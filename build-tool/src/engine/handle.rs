//! Task handles: how callers wait for a result produced on a worker thread.

use std::any::Any;
use std::fmt;
use std::sync::{Arc, Condvar, Mutex};

type PanicPayload = Box<dyn Any + Send + 'static>;

/// The error returned by [`JoinHandle::join`] when the task panicked instead
/// of returning normally.
#[derive(Debug)]
pub enum JoinError {
    /// The task panicked. Contains the panic message when it could be
    /// recovered as a `&str` or `String`.
    Panicked(String),
}

impl fmt::Display for JoinError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            JoinError::Panicked(msg) => write!(f, "task panicked: {msg}"),
        }
    }
}

impl std::error::Error for JoinError {}

pub(crate) fn panic_message(payload: PanicPayload) -> String {
    if let Some(s) = payload.downcast_ref::<&str>() {
        s.to_string()
    } else if let Some(s) = payload.downcast_ref::<String>() {
        s.clone()
    } else {
        "non-string panic payload".to_string()
    }
}

struct Shared<T> {
    state: Mutex<Option<Result<T, String>>>,
    cond: Condvar,
}

/// A handle to a task spawned on the runtime.
///
/// Dropping the handle without calling [`join`](JoinHandle::join) is fine;
/// the task still runs to completion, its result is simply discarded.
pub struct JoinHandle<T> {
    shared: Arc<Shared<T>>,
}

/// The producer side of a [`JoinHandle`], held internally by the runtime so
/// it can deliver the task's outcome once execution finishes.
pub(crate) struct ResultSetter<T> {
    shared: Arc<Shared<T>>,
}

impl<T> ResultSetter<T> {
    pub(crate) fn set(&self, result: Result<T, String>) {
        let mut guard = self.shared.state.lock().unwrap();
        *guard = Some(result);
        self.shared.cond.notify_all();
    }
}

pub(crate) fn new_handle_pair<T>() -> (ResultSetter<T>, JoinHandle<T>) {
    let shared = Arc::new(Shared {
        state: Mutex::new(None),
        cond: Condvar::new(),
    });
    (
        ResultSetter {
            shared: shared.clone(),
        },
        JoinHandle { shared },
    )
}

impl<T> JoinHandle<T> {
    /// Blocks the calling thread until the task completes, returning its
    /// result or the panic it raised.
    pub fn join(self) -> Result<T, JoinError> {
        let mut guard = self.shared.state.lock().unwrap();
        while guard.is_none() {
            guard = self.shared.cond.wait(guard).unwrap();
        }
        match guard.take().unwrap() {
            Ok(value) => Ok(value),
            Err(msg) => Err(JoinError::Panicked(msg)),
        }
    }

    /// Returns `true` if the task has already completed (or panicked).
    pub fn is_finished(&self) -> bool {
        self.shared.state.lock().unwrap().is_some()
    }
}
