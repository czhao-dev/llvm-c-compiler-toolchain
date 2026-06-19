//! `build-tool`: a dependency-graph build tool, supporting a subset of
//! Makefile syntax and semantics, built on a vendored work-stealing scheduler.
//! See the README for supported / not-yet-supported features.

use std::path::Path;
use std::process::ExitCode;

use build_tool::cli::{self, CliError};
use build_tool::makefile;
use build_tool::planner::{self, BuildStatus};
use build_tool::{run_graph, Runtime};

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
    eprintln!("build-tool: {e}");
    ExitCode::from(2)
}

/// Discovers + parses the makefile, plans, executes, and returns a process
/// exit code: 0 success, 1 a goal failed (or was skipped due to a failed
/// dependency), 2 a usage/parse/plan error.
fn run(args: &cli::Args, cwd: &Path) -> i32 {
    let Some(makefile_path) = makefile::discover(cwd) else {
        eprintln!("build-tool: no makefile found in {}", cwd.display());
        return 2;
    };

    let text = match std::fs::read_to_string(&makefile_path) {
        Ok(text) => text,
        Err(e) => {
            eprintln!(
                "build-tool: failed to read {}: {e}",
                makefile_path.display()
            );
            return 2;
        }
    };

    let parsed = match makefile::parse(&text) {
        Ok(p) => p,
        Err(e) => {
            eprintln!("build-tool: {}: {e}", makefile_path.display());
            return 2;
        }
    };

    let goals: Vec<String> = if !args.targets.is_empty() {
        args.targets.clone()
    } else if let Some(goal) = parsed.default_goal() {
        vec![goal.to_string()]
    } else {
        eprintln!("build-tool: no targets specified and no default goal in makefile");
        return 2;
    };

    let (graph, goal_ids) = match planner::plan(&parsed, &goals, args.keep_going) {
        Ok(result) => result,
        Err(e) => {
            eprintln!("build-tool: {e}");
            return 2;
        }
    };

    let runtime = Runtime::new(args.jobs);
    let results = run_graph(&runtime, graph);
    runtime.shutdown();

    let mut any_failed = false;
    for (goal, id) in goals.iter().zip(goal_ids.iter()) {
        match results.get::<BuildStatus>(*id) {
            BuildStatus::UpToDate => println!("build-tool: '{goal}' is up to date."),
            BuildStatus::Built => {}
            BuildStatus::Failed => {
                eprintln!("build-tool: *** [{goal}] failed");
                any_failed = true;
            }
            BuildStatus::Skipped => {
                eprintln!("build-tool: *** [{goal}] skipped due to a failed dependency");
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
