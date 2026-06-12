//! `parallel-make`: a dependency-graph build tool, supporting a subset of
//! Makefile syntax and semantics, built on a vendored work-stealing scheduler.
//! See the README for supported / not-yet-supported features.

use std::path::Path;
use std::process::ExitCode;

use parallel_make::cli::{self, CliError};
use parallel_make::makefile;
use parallel_make::planner::{self, BuildStatus};
use parallel_make::{run_graph, Runtime};

fn main() -> ExitCode {
    let args: Vec<String> = std::env::args().skip(1).collect();
    let parsed_args = match cli::parse(args) {
        Ok(a) => a,
        Err(e) => return usage_error(e),
    };

    let cwd = std::env::current_dir().expect("cannot read current directory");
    ExitCode::from(run(&parsed_args, &cwd) as u8)
}

fn usage_error(e: CliError) -> ExitCode {
    eprintln!("parallel-make: {e}");
    ExitCode::from(2)
}

/// Discovers + parses the makefile, plans, executes, and returns a process
/// exit code: 0 success, 1 a goal failed (or was skipped due to a failed
/// dependency), 2 a usage/parse/plan error.
fn run(args: &cli::Args, cwd: &Path) -> i32 {
    let Some(makefile_path) = makefile::discover(cwd) else {
        eprintln!("parallel-make: no makefile found in {}", cwd.display());
        return 2;
    };

    let text = match std::fs::read_to_string(&makefile_path) {
        Ok(text) => text,
        Err(e) => {
            eprintln!(
                "parallel-make: failed to read {}: {e}",
                makefile_path.display()
            );
            return 2;
        }
    };

    let parsed = match makefile::parse(&text) {
        Ok(p) => p,
        Err(e) => {
            eprintln!("parallel-make: {}: {e}", makefile_path.display());
            return 2;
        }
    };

    let goals: Vec<String> = if !args.targets.is_empty() {
        args.targets.clone()
    } else if let Some(goal) = parsed.default_goal() {
        vec![goal.to_string()]
    } else {
        eprintln!("parallel-make: no targets specified and no default goal in makefile");
        return 2;
    };

    let (graph, goal_ids) = match planner::plan(&parsed, &goals, args.keep_going) {
        Ok(result) => result,
        Err(e) => {
            eprintln!("parallel-make: {e}");
            return 2;
        }
    };

    let runtime = Runtime::new(args.jobs);
    let results = run_graph(&runtime, graph);
    runtime.shutdown();

    let mut any_failed = false;
    for (goal, id) in goals.iter().zip(goal_ids.iter()) {
        match results.get::<BuildStatus>(*id) {
            BuildStatus::UpToDate => println!("parallel-make: '{goal}' is up to date."),
            BuildStatus::Built => {}
            BuildStatus::Failed => {
                eprintln!("parallel-make: *** [{goal}] failed");
                any_failed = true;
            }
            BuildStatus::Skipped => {
                eprintln!("parallel-make: *** [{goal}] skipped due to a failed dependency");
                any_failed = true;
            }
        }
    }

    if any_failed {
        1
    } else {
        0
    }
}
