use std::path::PathBuf;

use clap::{Parser, Subcommand};

use crate::analyzer::analyze_paths;
use crate::config::{load_config, Config};

#[derive(Parser)]
#[command(name = "c-static-analyzer", version)]
struct Cli {
    #[command(subcommand)]
    command: Command,
}

#[derive(Subcommand)]
enum Command {
    /// Scan C files for issues
    Scan(ScanArgs),
}

#[derive(clap::Args)]
struct ScanArgs {
    /// Files or directories to scan
    paths: Vec<String>,

    /// Cyclomatic complexity threshold
    #[arg(long = "max-complexity")]
    max_complexity: Option<i64>,

    /// Control flow nesting depth threshold
    #[arg(long = "max-nesting")]
    max_nesting: Option<i64>,

    /// Comma-separated list of rule IDs to enable (default: all)
    #[arg(long, value_name = "SA001,SA002")]
    select: Option<String>,

    /// Glob pattern to exclude; can be passed multiple times
    #[arg(long = "exclude", value_name = "PATTERN")]
    exclude: Vec<String>,

    /// Ignore .c-static-analyzer.toml configuration
    #[arg(long = "no-config")]
    no_config: bool,
}

fn build_config(args: &ScanArgs) -> Config {
    let mut config = if args.no_config {
        Config::default()
    } else {
        load_config(&std::env::current_dir().expect("cwd must be accessible"))
    };
    if let Some(max_complexity) = args.max_complexity {
        config.max_complexity = max_complexity;
    }
    if let Some(max_nesting) = args.max_nesting {
        config.max_nesting = max_nesting;
    }
    if let Some(select) = &args.select {
        config.enabled_rules = select
            .split(',')
            .map(str::trim)
            .filter(|s| !s.is_empty())
            .map(str::to_string)
            .collect();
    }
    config.exclude.extend(args.exclude.iter().cloned());
    config
}

fn run_scan(args: ScanArgs) -> i32 {
    let raw_paths = if args.paths.is_empty() {
        vec![".".to_string()]
    } else {
        args.paths.clone()
    };
    let paths: Vec<PathBuf> = raw_paths.iter().map(PathBuf::from).collect();

    if let Some(missing) = paths.iter().find(|path| !path.exists()) {
        eprintln!("error: path not found: {}", missing.display());
        return 2;
    }

    let config = build_config(&args);
    let diagnostics = analyze_paths(&paths, &config);
    for diagnostic in &diagnostics {
        println!("{diagnostic}");
    }

    if diagnostics.is_empty() {
        0
    } else {
        eprintln!("\n{} issue(s) found.", diagnostics.len());
        1
    }
}

pub fn run() -> i32 {
    let cli = Cli::parse();
    match cli.command {
        Command::Scan(args) => run_scan(args),
    }
}
