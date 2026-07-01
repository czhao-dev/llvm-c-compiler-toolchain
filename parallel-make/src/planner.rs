//! Turns parsed Makefile rules into a [`TaskGraph`] and executes recipes
//! with fail-fast / keep-going semantics, propagated through node outputs
//! rather than through a cancellation token (recipes run to completion;
//! what we need to skip is *unstarted* downstream work, which a plain
//! status value handles more directly).

use std::collections::HashMap;
use std::fmt;
use std::path::Path;
use std::process::Command;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Arc;

use crate::engine::{DependencyResults, NodeId, TaskGraph};
use crate::makefile::{ParsedMakefile, Rule};

/// The outcome of evaluating a single graph node (one Makefile target).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum BuildStatus {
    /// The target's file is newer than all of its prerequisites; nothing
    /// was run.
    UpToDate,
    /// The recipe ran and every command succeeded.
    Built,
    /// The recipe ran and a command exited non-zero (or couldn't launch).
    Failed,
    /// Not attempted, because a prerequisite failed (or the run already
    /// failed elsewhere and `-k` wasn't given).
    Skipped,
}

/// An error discovered while resolving rule names into a dependency graph,
/// before any recipe has run. These always abort the build, regardless of
/// `-k` / keep-going.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum PlanError {
    NoRule {
        target: String,
        needed_by: Option<String>,
    },
    Cycle(Vec<String>),
}

impl fmt::Display for PlanError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            PlanError::NoRule {
                target,
                needed_by: Some(parent),
            } => write!(f, "No rule to make target '{target}', needed by '{parent}'"),
            PlanError::NoRule {
                target,
                needed_by: None,
            } => write!(f, "No rule to make target '{target}'"),
            PlanError::Cycle(chain) => {
                write!(f, "Circular dependency dropped: {}", chain.join(" -> "))
            }
        }
    }
}

impl std::error::Error for PlanError {}

/// Builds a [`TaskGraph`] whose nodes correspond to `goals` and everything
/// they (transitively) depend on, ready to hand to [`crate::run_graph`].
/// Returns the graph along with the `NodeId` of each requested goal, in
/// the same order as `goals`.
pub fn plan(
    rules: &ParsedMakefile,
    goals: &[String],
    keep_going: bool,
) -> Result<(TaskGraph, Vec<NodeId>), PlanError> {
    let mut rules_by_target: HashMap<&str, &Rule> = HashMap::new();
    for rule in &rules.rules {
        rules_by_target.entry(rule.target.as_str()).or_insert(rule);
    }

    let mut resolver = Resolver {
        rules_by_target,
        graph: TaskGraph::new(),
        memo: HashMap::new(),
        visiting: Vec::new(),
        fail_flag: Arc::new(AtomicBool::new(false)),
        keep_going,
    };

    let mut node_ids = Vec::with_capacity(goals.len());
    for goal in goals {
        node_ids.push(resolver.resolve(goal, None)?);
    }

    Ok((resolver.graph, node_ids))
}

struct Resolver<'a> {
    rules_by_target: HashMap<&'a str, &'a Rule>,
    graph: TaskGraph,
    memo: HashMap<String, NodeId>,
    visiting: Vec<String>,
    fail_flag: Arc<AtomicBool>,
    keep_going: bool,
}

impl<'a> Resolver<'a> {
    /// Resolves `name` to a `NodeId`, recursively resolving its
    /// prerequisites first (so `add_task_after`'s "deps must already
    /// exist" requirement is satisfied) and memoizing so a name shared by
    /// multiple parents (diamond dependencies) only gets one node.
    fn resolve(&mut self, name: &str, needed_by: Option<&str>) -> Result<NodeId, PlanError> {
        if let Some(pos) = self.visiting.iter().position(|n| n == name) {
            let mut chain: Vec<String> = self.visiting[pos..].to_vec();
            chain.push(name.to_string());
            return Err(PlanError::Cycle(chain));
        }
        if let Some(&id) = self.memo.get(name) {
            return Ok(id);
        }

        self.visiting.push(name.to_string());
        let result = self.resolve_uncached(name, needed_by);
        self.visiting.pop();

        if let Ok(id) = result {
            self.memo.insert(name.to_string(), id);
        }
        result
    }

    fn resolve_uncached(
        &mut self,
        name: &str,
        needed_by: Option<&str>,
    ) -> Result<NodeId, PlanError> {
        let Some(rule) = self.rules_by_target.get(name).copied() else {
            return if Path::new(name).is_file() {
                Ok(self.graph.add_task(|| BuildStatus::UpToDate))
            } else {
                Err(PlanError::NoRule {
                    target: name.to_string(),
                    needed_by: needed_by.map(str::to_string),
                })
            };
        };

        let mut prereq_ids = Vec::with_capacity(rule.prereqs.len());
        for prereq in &rule.prereqs {
            prereq_ids.push(self.resolve(prereq, Some(name))?);
        }

        let job = RecipeJob {
            target: rule.target.clone(),
            recipe: rule.recipe.clone(),
            phony: rule.phony,
            prereq_names: rule.prereqs.clone(),
            prereq_ids: prereq_ids.clone(),
        };
        let fail_flag = self.fail_flag.clone();
        let keep_going = self.keep_going;

        Ok(self
            .graph
            .add_task_after(prereq_ids, move |results: &DependencyResults| {
                job.run(results, &fail_flag, keep_going)
            }))
    }
}

/// Everything one target's recipe closure needs, captured by value so it
/// can be moved into a `'static` task body.
struct RecipeJob {
    target: String,
    recipe: Vec<String>,
    phony: bool,
    prereq_names: Vec<String>,
    prereq_ids: Vec<NodeId>,
}

impl RecipeJob {
    fn run(
        &self,
        results: &DependencyResults,
        fail_flag: &Arc<AtomicBool>,
        keep_going: bool,
    ) -> BuildStatus {
        if fail_flag.load(Ordering::Relaxed) && !keep_going {
            return BuildStatus::Skipped;
        }

        let mut any_prereq_failed = false;
        let mut any_prereq_built = false;
        for &id in &self.prereq_ids {
            // A dependency's own closure panicking would otherwise panic
            // here too via `get`'s `Err` branch; treat that the same as a
            // normal failure instead of letting the panic cascade further.
            match std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
                results.get::<BuildStatus>(id)
            })) {
                Ok(BuildStatus::Failed) | Ok(BuildStatus::Skipped) => any_prereq_failed = true,
                Ok(BuildStatus::Built) => any_prereq_built = true,
                Ok(BuildStatus::UpToDate) => {}
                Err(_) => any_prereq_failed = true,
            }
        }

        if any_prereq_failed {
            if !keep_going {
                fail_flag.store(true, Ordering::Relaxed);
            }
            return BuildStatus::Skipped;
        }

        let stale = self.phony || any_prereq_built || is_stale(&self.target, &self.prereq_names);
        if !stale {
            return BuildStatus::UpToDate;
        }

        for line in &self.recipe {
            println!("{line}");
            match Command::new("sh").arg("-c").arg(line).status() {
                Ok(status) if status.success() => continue,
                _ => {
                    if !keep_going {
                        fail_flag.store(true, Ordering::Relaxed);
                    }
                    return BuildStatus::Failed;
                }
            }
        }

        BuildStatus::Built
    }
}

/// Computed at execution time (not plan time): a target is stale if it's
/// missing, or any prerequisite file is newer than it. Metadata errors
/// (e.g. a prerequisite that vanished) are treated as "stale" rather than
/// propagated as an error.
fn is_stale(target: &str, prereq_names: &[String]) -> bool {
    let target_mtime = match std::fs::metadata(target).and_then(|m| m.modified()) {
        Ok(mtime) => mtime,
        Err(_) => return true,
    };
    for prereq in prereq_names {
        let prereq_mtime = match std::fs::metadata(prereq).and_then(|m| m.modified()) {
            Ok(mtime) => mtime,
            Err(_) => return true,
        };
        if prereq_mtime > target_mtime {
            return true;
        }
    }
    false
}
