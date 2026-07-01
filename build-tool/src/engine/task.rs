//! The type-erased unit of work that flows through every queue.

/// A boxed, one-shot unit of work. All queueing and stealing infrastructure
/// operates on this single concrete type so that local deques, the global
/// injector, and stealers never need to be generic over the user's task
/// closures or return types.
pub(crate) type Job = Box<dyn FnOnce() + Send + 'static>;
