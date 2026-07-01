//! `parallel-make`: a dependency-graph-aware build tool following GNU Make's
//! documented dependency semantics, built on a vendored work-stealing thread
//! pool and DAG scheduler (see `engine/`).

pub mod cli;
pub mod engine;
pub mod makefile;
pub mod planner;

pub use engine::{run_graph, Runtime};
