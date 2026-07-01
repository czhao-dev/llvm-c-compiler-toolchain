//! Dependency-aware (DAG) task scheduling on top of [`Runtime::spawn`].
//!
//! A [`TaskGraph`] is built up by adding nodes; each node can only depend on
//! nodes added earlier, so a [`NodeId`] is always a backward reference and
//! cycles are impossible by construction. [`run_graph`] schedules nodes as
//! soon as their dependencies finish and blocks until the whole graph
//! completes.

use std::any::Any;
use std::sync::mpsc;
use std::sync::{Arc, Mutex};

use crate::engine::handle::panic_message;
use crate::engine::priority::Priority;
use crate::engine::runtime::Runtime;

/// An opaque reference to a node within a single [`TaskGraph`].
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub struct NodeId(usize);

type NodeOutput = Box<dyn Any + Send>;
type NodeResult = Result<NodeOutput, String>;
type ResultSlots = Arc<Vec<Mutex<Option<NodeResult>>>>;

enum NodeBody {
    Root(Box<dyn FnOnce() -> NodeOutput + Send + 'static>),
    Dependent(Box<dyn FnOnce(&DependencyResults) -> NodeOutput + Send + 'static>),
}

struct NodeDef {
    deps: Vec<NodeId>,
    body: NodeBody,
}

/// A DAG of tasks. Build it with [`add_task`](Self::add_task) and
/// [`add_task_after`](Self::add_task_after), then execute it with
/// [`run_graph`].
#[derive(Default)]
pub struct TaskGraph {
    nodes: Vec<NodeDef>,
}

impl TaskGraph {
    /// Creates an empty graph.
    pub fn new() -> Self {
        Self { nodes: Vec::new() }
    }

    /// Adds a task with no dependencies.
    pub fn add_task<F, T>(&mut self, f: F) -> NodeId
    where
        F: FnOnce() -> T + Send + 'static,
        T: Send + 'static,
    {
        let id = NodeId(self.nodes.len());
        self.nodes.push(NodeDef {
            deps: Vec::new(),
            body: NodeBody::Root(Box::new(move || Box::new(f()) as NodeOutput)),
        });
        id
    }

    /// Adds a task that only becomes eligible to run once every node in
    /// `deps` has completed. The task body receives a [`DependencyResults`]
    /// view it can use to read its dependencies' outputs.
    pub fn add_task_after<F, T>(&mut self, deps: impl IntoIterator<Item = NodeId>, f: F) -> NodeId
    where
        F: FnOnce(&DependencyResults) -> T + Send + 'static,
        T: Send + 'static,
    {
        let id = NodeId(self.nodes.len());
        self.nodes.push(NodeDef {
            deps: deps.into_iter().collect(),
            body: NodeBody::Dependent(Box::new(move |results| Box::new(f(results)) as NodeOutput)),
        });
        id
    }
}

/// A read-only view over completed dependency outputs, handed to a
/// dependent task.
pub struct DependencyResults {
    slots: ResultSlots,
}

impl DependencyResults {
    /// Returns a clone of the output produced by `id`.
    ///
    /// # Panics
    /// Panics if `id`'s task panicked instead of completing, or if `T`
    /// does not match the type that task actually returned.
    pub fn get<T: Clone + 'static>(&self, id: NodeId) -> T {
        let guard = self.slots[id.0].lock().unwrap();
        match guard.as_ref().expect("dependency has not completed yet") {
            Ok(value) => value
                .downcast_ref::<T>()
                .expect("dependency result type mismatch")
                .clone(),
            Err(msg) => panic!("dependency task panicked: {msg}"),
        }
    }
}

/// Runs every node of `graph` on `runtime`, respecting dependency order,
/// and blocks until the entire graph has finished executing.
///
/// Nodes with no unmet dependencies are submitted immediately; each
/// remaining node is submitted as soon as its last prerequisite completes.
/// This lets independent branches of the graph run concurrently while
/// still using the runtime's normal work-stealing scheduling underneath.
pub fn run_graph(runtime: &Runtime, mut graph: TaskGraph) -> DependencyResults {
    let n = graph.nodes.len();
    let slots: ResultSlots = Arc::new((0..n).map(|_| Mutex::new(None)).collect());

    let mut dependents: Vec<Vec<usize>> = vec![Vec::new(); n];
    let mut remaining: Vec<usize> = Vec::with_capacity(n);
    for (i, node) in graph.nodes.iter().enumerate() {
        remaining.push(node.deps.len());
        for dep in &node.deps {
            dependents[dep.0].push(i);
        }
    }

    let mut bodies: Vec<Option<NodeBody>> =
        graph.nodes.drain(..).map(|node| Some(node.body)).collect();
    let (tx, rx) = mpsc::channel::<usize>();

    let spawn_node = |idx: usize, bodies: &mut Vec<Option<NodeBody>>| {
        let body = bodies[idx].take().expect("node scheduled more than once");
        let slots = slots.clone();
        let tx = tx.clone();
        runtime.spawn_with_priority(Priority::Normal, move || {
            let outcome = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| match body {
                NodeBody::Root(f) => f(),
                NodeBody::Dependent(f) => {
                    let results = DependencyResults {
                        slots: slots.clone(),
                    };
                    f(&results)
                }
            }));
            *slots[idx].lock().unwrap() = Some(outcome.map_err(panic_message));
            let _ = tx.send(idx);
        });
    };

    for (i, &deps_left) in remaining.iter().enumerate() {
        if deps_left == 0 {
            spawn_node(i, &mut bodies);
        }
    }

    let mut finished = 0usize;
    while finished < n {
        let i = rx
            .recv()
            .expect("a graph node disconnected without reporting completion");
        finished += 1;
        for &dependent in &dependents[i] {
            remaining[dependent] -= 1;
            if remaining[dependent] == 0 {
                spawn_node(dependent, &mut bodies);
            }
        }
    }

    DependencyResults { slots }
}
