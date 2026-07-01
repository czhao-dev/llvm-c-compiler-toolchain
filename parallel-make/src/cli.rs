//! Hand-rolled command-line parsing for the `parallel-make` binary: just
//! enough flags for the "Core" feature set (`-j`, `-k`, target names). `-n`
//! and `-f` are recognized only so they can be rejected with a clear "not yet
//! supported" message instead of being silently misread as target names.

use std::fmt;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Args {
    pub targets: Vec<String>,
    pub jobs: usize,
    pub keep_going: bool,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum CliError {
    MissingJobsValue,
    InvalidJobsValue(String),
    NotYetSupported(String),
    UnknownFlag(String),
}

impl fmt::Display for CliError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            CliError::MissingJobsValue => write!(f, "-j requires a number"),
            CliError::InvalidJobsValue(v) => write!(f, "invalid -j value: '{v}'"),
            CliError::NotYetSupported(flag) => write!(f, "'{flag}' is not yet supported"),
            CliError::UnknownFlag(flag) => write!(f, "unknown flag '{flag}'"),
        }
    }
}

impl std::error::Error for CliError {}

/// Parses CLI arguments (excluding `argv[0]`). Default `jobs` is `1`,
/// matching GNU make's serial-unless-told-otherwise default.
pub fn parse(args: impl IntoIterator<Item = String>) -> Result<Args, CliError> {
    let mut targets = Vec::new();
    let mut jobs = 1usize;
    let mut keep_going = false;

    let mut iter = args.into_iter();
    while let Some(arg) = iter.next() {
        if arg == "-n" || arg == "-f" {
            return Err(CliError::NotYetSupported(arg));
        } else if arg == "-k" || arg == "--keep-going" {
            keep_going = true;
        } else if arg == "-j" {
            let value = iter.next().ok_or(CliError::MissingJobsValue)?;
            jobs = parse_jobs(&value)?;
        } else if let Some(value) = arg.strip_prefix("-j") {
            jobs = parse_jobs(value)?;
        } else if arg.starts_with('-') && arg != "-" {
            return Err(CliError::UnknownFlag(arg));
        } else {
            targets.push(arg);
        }
    }

    Ok(Args {
        targets,
        jobs,
        keep_going,
    })
}

fn parse_jobs(value: &str) -> Result<usize, CliError> {
    let jobs: usize = value
        .parse()
        .map_err(|_| CliError::InvalidJobsValue(value.to_string()))?;
    if jobs == 0 {
        return Err(CliError::InvalidJobsValue(value.to_string()));
    }
    Ok(jobs)
}
