//! The public entry point: a fixed-size, work-stealing thread pool.

use std::sync::atomic::{AtomicBool, AtomicUsize, Ordering};
use std::sync::{Arc, Mutex};
use std::thread;

use crossbeam_deque::Stealer;

use crate::engine::cancellation::{CancellationContext, CancellationToken};
use crate::engine::handle::{new_handle_pair, panic_message, JoinHandle};
use crate::engine::metrics::{Metrics, RuntimeMetrics};
use crate::engine::priority::{Priority, PRIORITY_COUNT};
use crate::engine::steal::{build_worker_queues, Injectors};
use crate::engine::task::Job;
use crate::engine::worker::{run_worker, IdleSignal};

/// State shared between the `Runtime` handle and every worker thread.
pub(crate) struct RuntimeShared {
    pub(crate) injectors: Injectors,
    pub(crate) stealers: Vec<[Stealer<Job>; PRIORITY_COUNT]>,
    pub(crate) pending: AtomicUsize,
    pub(crate) shutdown: AtomicBool,
    pub(crate) idle: IdleSignal,
    pub(crate) metrics: Metrics,
}

/// A fixed-size pool of worker threads that execute submitted tasks using
/// per-priority work-stealing queues.
pub struct Runtime {
    shared: Arc<RuntimeShared>,
    workers: Mutex<Option<Vec<thread::JoinHandle<()>>>>,
    num_workers: usize,
}

impl Runtime {
    /// Creates a runtime with `num_workers` worker threads.
    ///
    /// # Panics
    /// Panics if `num_workers` is zero.
    pub fn new(num_workers: usize) -> Self {
        assert!(
            num_workers > 0,
            "a runtime needs at least one worker thread"
        );

        let (locals, stealers) = build_worker_queues(num_workers);
        let shared = Arc::new(RuntimeShared {
            injectors: Injectors::new(),
            stealers,
            pending: AtomicUsize::new(0),
            shutdown: AtomicBool::new(false),
            idle: IdleSignal::new(),
            metrics: Metrics::default(),
        });

        let workers = locals
            .into_iter()
            .enumerate()
            .map(|(id, local)| {
                let shared = shared.clone();
                thread::Builder::new()
                    .name(format!("worker-{id}"))
                    .spawn(move || run_worker(id, local, shared))
                    .expect("failed to spawn worker thread")
            })
            .collect();

        Runtime {
            shared,
            workers: Mutex::new(Some(workers)),
            num_workers,
        }
    }

    /// The number of worker threads in the pool.
    pub fn num_workers(&self) -> usize {
        self.num_workers
    }

    /// A snapshot of runtime activity counters.
    pub fn metrics(&self) -> RuntimeMetrics {
        self.shared.metrics.snapshot()
    }

    /// Submits a task at [`Priority::Normal`] and returns a handle to its
    /// result.
    pub fn spawn<F, T>(&self, f: F) -> JoinHandle<T>
    where
        F: FnOnce() -> T + Send + 'static,
        T: Send + 'static,
    {
        self.spawn_with_priority(Priority::Normal, f)
    }

    /// Submits a task at the given priority and returns a handle to its
    /// result. Higher-priority tasks are preferred by workers at every
    /// stage of scheduling: local queue, global injector, and steal.
    pub fn spawn_with_priority<F, T>(&self, priority: Priority, f: F) -> JoinHandle<T>
    where
        F: FnOnce() -> T + Send + 'static,
        T: Send + 'static,
    {
        let (setter, handle) = new_handle_pair();
        let shared = self.shared.clone();
        self.submit(priority, move || {
            match std::panic::catch_unwind(std::panic::AssertUnwindSafe(f)) {
                Ok(value) => setter.set(Ok(value)),
                Err(payload) => {
                    shared.metrics.record_panicked();
                    setter.set(Err(panic_message(payload)));
                }
            }
        });
        handle
    }

    /// Submits a cooperatively cancellable task at [`Priority::Normal`].
    /// The task body receives a [`CancellationContext`] it can poll to
    /// learn whether `token` has been cancelled.
    pub fn spawn_cancellable<F, T>(&self, token: CancellationToken, f: F) -> JoinHandle<T>
    where
        F: FnOnce(&CancellationContext) -> T + Send + 'static,
        T: Send + 'static,
    {
        self.spawn_cancellable_with_priority(Priority::Normal, token, f)
    }

    /// Like [`spawn_cancellable`](Self::spawn_cancellable), at an explicit
    /// priority.
    pub fn spawn_cancellable_with_priority<F, T>(
        &self,
        priority: Priority,
        token: CancellationToken,
        f: F,
    ) -> JoinHandle<T>
    where
        F: FnOnce(&CancellationContext) -> T + Send + 'static,
        T: Send + 'static,
    {
        let (setter, handle) = new_handle_pair();
        let shared = self.shared.clone();
        let ctx = token.context();
        self.submit(priority, move || {
            match std::panic::catch_unwind(std::panic::AssertUnwindSafe(move || f(&ctx))) {
                Ok(value) => setter.set(Ok(value)),
                Err(payload) => {
                    shared.metrics.record_panicked();
                    setter.set(Err(panic_message(payload)));
                }
            }
        });
        handle
    }

    /// Requests a graceful shutdown: stops accepting the premise that more
    /// work is coming, lets every already-submitted task run to completion,
    /// then joins all worker threads. Safe to call multiple times.
    pub fn shutdown(&self) {
        self.shared.shutdown.store(true, Ordering::SeqCst);
        self.shared.idle.notify();
        self.join_workers();
    }

    fn join_workers(&self) {
        if let Some(workers) = self.workers.lock().unwrap().take() {
            for worker in workers {
                let _ = worker.join();
            }
        }
    }

    fn submit(&self, priority: Priority, job: impl FnOnce() + Send + 'static) {
        self.shared.metrics.record_submitted();
        self.shared.pending.fetch_add(1, Ordering::SeqCst);
        let shared = self.shared.clone();
        let wrapped: Job = Box::new(move || {
            job();
            shared.pending.fetch_sub(1, Ordering::SeqCst);
            shared.metrics.record_completed();
            shared.idle.notify();
        });
        self.shared.injectors.push(priority, wrapped);
        self.shared.idle.notify();
    }
}

impl Drop for Runtime {
    fn drop(&mut self) {
        self.shared.shutdown.store(true, Ordering::SeqCst);
        self.shared.idle.notify();
        self.join_workers();
    }
}
