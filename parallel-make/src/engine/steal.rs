//! Per-priority work-stealing queue infrastructure.
//!
//! Each priority class gets its own independent set of queues so that a
//! flood of `Background` work can never delay a `High` priority task:
//! workers always drain higher priority levels first, at every stage of the
//! search (local queue, global injector, peer steal).

use crossbeam_deque::{Injector, Steal, Stealer, Worker as LocalDeque};

use crate::engine::priority::{Priority, PRIORITY_COUNT, PRIORITY_ORDER};
use crate::engine::task::Job;

/// One global, multi-producer injector per priority level. External callers
/// (anyone holding a `Runtime` handle) push here; idle workers pull batches
/// of work out of the injector into their own local deque.
pub(crate) struct Injectors {
    levels: [Injector<Job>; PRIORITY_COUNT],
}

impl Injectors {
    pub(crate) fn new() -> Self {
        Self {
            levels: [Injector::new(), Injector::new(), Injector::new()],
        }
    }

    pub(crate) fn push(&self, priority: Priority, job: Job) {
        self.levels[priority as usize].push(job);
    }
}

/// The local (single-owner) and stealer (shared) ends of one worker's
/// per-priority deques.
pub(crate) struct LocalQueues {
    deques: [LocalDeque<Job>; PRIORITY_COUNT],
}

impl LocalQueues {
    fn new() -> Self {
        Self {
            deques: [
                LocalDeque::new_fifo(),
                LocalDeque::new_fifo(),
                LocalDeque::new_fifo(),
            ],
        }
    }

    pub(crate) fn stealers(&self) -> [Stealer<Job>; PRIORITY_COUNT] {
        [
            self.deques[0].stealer(),
            self.deques[1].stealer(),
            self.deques[2].stealer(),
        ]
    }

    fn pop_local(&self, priority: Priority) -> Option<Job> {
        self.deques[priority as usize].pop()
    }

    fn steal_from_injector(&self, injectors: &Injectors, priority: Priority) -> Steal<Job> {
        injectors.levels[priority as usize].steal_batch_and_pop(&self.deques[priority as usize])
    }
}

/// Builds one [`LocalQueues`] per worker plus the matching set of stealer
/// handles every worker needs in order to steal from every other worker.
pub(crate) fn build_worker_queues(
    num_workers: usize,
) -> (Vec<LocalQueues>, Vec<[Stealer<Job>; PRIORITY_COUNT]>) {
    let locals: Vec<LocalQueues> = (0..num_workers).map(|_| LocalQueues::new()).collect();
    let stealers: Vec<[Stealer<Job>; PRIORITY_COUNT]> =
        locals.iter().map(|l| l.stealers()).collect();
    (locals, stealers)
}

/// Outcome of a successful search, distinguishing a steal from a peer
/// (interesting for metrics) from work pulled from the worker's own queue
/// or the shared injector.
pub(crate) enum Found {
    Job(Job),
    JobStolen(Job),
}

/// Searches, in priority order, this worker's own local queues, then the
/// global injectors, then every peer worker's local queue.
pub(crate) fn find_task(
    worker_id: usize,
    local: &LocalQueues,
    injectors: &Injectors,
    peers: &[[Stealer<Job>; PRIORITY_COUNT]],
) -> Option<Found> {
    for &priority in PRIORITY_ORDER.iter() {
        if let Some(job) = local.pop_local(priority) {
            return Some(Found::Job(job));
        }
    }

    for &priority in PRIORITY_ORDER.iter() {
        loop {
            match local.steal_from_injector(injectors, priority) {
                Steal::Success(job) => return Some(Found::Job(job)),
                Steal::Retry => continue,
                Steal::Empty => break,
            }
        }
    }

    for &priority in PRIORITY_ORDER.iter() {
        for (peer_id, peer) in peers.iter().enumerate() {
            if peer_id == worker_id {
                continue;
            }
            loop {
                match peer[priority as usize].steal() {
                    Steal::Success(job) => return Some(Found::JobStolen(job)),
                    Steal::Retry => continue,
                    Steal::Empty => break,
                }
            }
        }
    }

    None
}
