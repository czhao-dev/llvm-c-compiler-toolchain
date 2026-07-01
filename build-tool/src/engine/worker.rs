//! Worker thread main loop.

use std::sync::atomic::Ordering;
use std::sync::{Arc, Condvar, Mutex};
use std::time::Duration;

use crate::engine::runtime::RuntimeShared;
use crate::engine::steal::{find_task, Found, LocalQueues};

/// How long an idle worker sleeps between polls when it has no work and
/// hasn't been woken by a doorbell notification. Short enough to keep
/// scheduling latency low, long enough to keep idle CPU use negligible.
const IDLE_POLL_INTERVAL: Duration = Duration::from_millis(1);

/// A doorbell used to wake idle workers promptly when new work arrives,
/// with a bounded-wait fallback so a missed notification (a benign race
/// between "go to sleep" and "a new task was just pushed") can never cause
/// a permanent stall.
pub(crate) struct IdleSignal {
    mutex: Mutex<()>,
    condvar: Condvar,
}

impl IdleSignal {
    pub(crate) fn new() -> Self {
        Self {
            mutex: Mutex::new(()),
            condvar: Condvar::new(),
        }
    }

    pub(crate) fn notify(&self) {
        let _guard = self.mutex.lock().unwrap();
        self.condvar.notify_all();
    }

    fn wait(&self) {
        let guard = self.mutex.lock().unwrap();
        let _ = self
            .condvar
            .wait_timeout(guard, IDLE_POLL_INTERVAL)
            .unwrap();
    }
}

pub(crate) fn run_worker(id: usize, local: LocalQueues, shared: Arc<RuntimeShared>) {
    loop {
        match find_task(id, &local, &shared.injectors, &shared.stealers) {
            Some(Found::Job(job)) => {
                job();
            }
            Some(Found::JobStolen(job)) => {
                shared.metrics.record_stolen();
                job();
            }
            None => {
                if shared.shutdown.load(Ordering::Acquire)
                    && shared.pending.load(Ordering::Acquire) == 0
                {
                    break;
                }
                shared.idle.wait();
            }
        }
    }
}
