use std::time::Duration;

use parallel_make::makefile::{ParsedMakefile, Rule};
use parallel_make::planner::{self, BuildStatus, PlanError};
use parallel_make::{run_graph, Runtime};

fn scratch_dir(tag: &str) -> std::path::PathBuf {
    let dir = std::env::temp_dir().join(format!(
        "parallel-make-planner-{tag}-{}",
        std::process::id()
    ));
    let _ = std::fs::remove_dir_all(&dir);
    std::fs::create_dir_all(&dir).unwrap();
    dir
}

fn rule(target: &str, prereqs: &[&str], recipe: &[&str], phony: bool) -> Rule {
    Rule {
        target: target.to_string(),
        prereqs: prereqs.iter().map(|s| s.to_string()).collect(),
        recipe: recipe.iter().map(|s| s.to_string()).collect(),
        phony,
    }
}

fn parsed(rules: Vec<Rule>) -> ParsedMakefile {
    ParsedMakefile {
        rules,
        phony_targets: Default::default(),
    }
}

#[test]
fn diamond_dependency_builds_shared_node_once() {
    let dir = scratch_dir("diamond");
    let shared = dir.join("shared.out");
    let counter = dir.join("counter.txt");
    let a = dir.join("a.out");
    let b = dir.join("b.out");

    let rules = vec![
        rule(
            shared.to_str().unwrap(),
            &[],
            &[
                &format!("echo run >> {}", counter.display()),
                &format!("touch {}", shared.display()),
            ],
            false,
        ),
        rule(
            a.to_str().unwrap(),
            &[shared.to_str().unwrap()],
            &[&format!("touch {}", a.display())],
            false,
        ),
        rule(
            b.to_str().unwrap(),
            &[shared.to_str().unwrap()],
            &[&format!("touch {}", b.display())],
            false,
        ),
    ];

    let goals = vec![
        a.to_str().unwrap().to_string(),
        b.to_str().unwrap().to_string(),
    ];
    let (graph, ids) = planner::plan(&parsed(rules), &goals, false).unwrap();
    let runtime = Runtime::new(4);
    let results = run_graph(&runtime, graph);
    runtime.shutdown();

    for id in &ids {
        assert_eq!(results.get::<BuildStatus>(*id), BuildStatus::Built);
    }
    let contents = std::fs::read_to_string(&counter).unwrap();
    assert_eq!(
        contents.lines().count(),
        1,
        "a dependency shared by two parents should only build once"
    );

    std::fs::remove_dir_all(&dir).unwrap();
}

#[test]
fn cycle_is_detected() {
    let rules = vec![rule("a", &["b"], &[], false), rule("b", &["a"], &[], false)];
    match planner::plan(&parsed(rules), &["a".to_string()], false) {
        Err(PlanError::Cycle(chain)) => {
            assert!(chain.contains(&"a".to_string()));
            assert!(chain.contains(&"b".to_string()));
        }
        other => panic!("expected Cycle, got {}", describe(other)),
    }
}

#[test]
fn missing_rule_and_file_is_an_error() {
    let rules = vec![rule("a", &["does-not-exist-anywhere"], &[], false)];
    match planner::plan(&parsed(rules), &["a".to_string()], false) {
        Err(PlanError::NoRule { target, needed_by }) => {
            assert_eq!(target, "does-not-exist-anywhere");
            assert_eq!(needed_by, Some("a".to_string()));
        }
        other => panic!("expected NoRule, got {}", describe(other)),
    }
}

fn describe(
    result: Result<
        (
            parallel_make::engine::TaskGraph,
            Vec<parallel_make::engine::NodeId>,
        ),
        PlanError,
    >,
) -> String {
    match result {
        Ok(_) => "Ok(..)".to_string(),
        Err(e) => format!("Err({e:?})"),
    }
}

#[test]
fn no_rule_but_existing_file_is_a_leaf() {
    let dir = scratch_dir("leaf");
    let source = dir.join("source.txt");
    std::fs::write(&source, "hi").unwrap();
    let target = dir.join("target.out");

    let rules = vec![rule(
        target.to_str().unwrap(),
        &[source.to_str().unwrap()],
        &[&format!("touch {}", target.display())],
        false,
    )];
    let goals = vec![target.to_str().unwrap().to_string()];
    let (graph, ids) = planner::plan(&parsed(rules), &goals, false).unwrap();
    let runtime = Runtime::new(2);
    let results = run_graph(&runtime, graph);
    runtime.shutdown();

    assert_eq!(results.get::<BuildStatus>(ids[0]), BuildStatus::Built);

    std::fs::remove_dir_all(&dir).unwrap();
}

#[test]
fn target_newer_than_prereq_is_up_to_date() {
    let dir = scratch_dir("uptodate");
    let prereq = dir.join("prereq.txt");
    let target = dir.join("target.out");
    let marker = dir.join("marker.txt");

    std::fs::write(&prereq, "p").unwrap();
    std::thread::sleep(Duration::from_millis(1100));
    std::fs::write(&target, "t").unwrap();

    let rules = vec![rule(
        target.to_str().unwrap(),
        &[prereq.to_str().unwrap()],
        &[&format!("touch {}", marker.display())],
        false,
    )];
    let goals = vec![target.to_str().unwrap().to_string()];
    let (graph, ids) = planner::plan(&parsed(rules), &goals, false).unwrap();
    let runtime = Runtime::new(2);
    let results = run_graph(&runtime, graph);
    runtime.shutdown();

    assert_eq!(results.get::<BuildStatus>(ids[0]), BuildStatus::UpToDate);
    assert!(!marker.exists(), "recipe should not have run");

    std::fs::remove_dir_all(&dir).unwrap();
}

#[test]
fn phony_target_rebuilds_even_when_newer_than_prereqs() {
    let dir = scratch_dir("phony");
    let prereq = dir.join("prereq.txt");
    let target = dir.join("target.out");

    std::fs::write(&prereq, "p").unwrap();
    std::thread::sleep(Duration::from_millis(1100));
    std::fs::write(&target, "t").unwrap();

    let rules = vec![rule(
        target.to_str().unwrap(),
        &[prereq.to_str().unwrap()],
        &["true"],
        true,
    )];
    let goals = vec![target.to_str().unwrap().to_string()];
    let (graph, ids) = planner::plan(&parsed(rules), &goals, false).unwrap();
    let runtime = Runtime::new(2);
    let results = run_graph(&runtime, graph);
    runtime.shutdown();

    assert_eq!(results.get::<BuildStatus>(ids[0]), BuildStatus::Built);

    std::fs::remove_dir_all(&dir).unwrap();
}

#[test]
fn empty_recipe_phony_target_succeeds_trivially() {
    let rules = vec![rule("noop", &[], &[], true)];
    let goals = vec!["noop".to_string()];
    let (graph, ids) = planner::plan(&parsed(rules), &goals, false).unwrap();
    let runtime = Runtime::new(1);
    let results = run_graph(&runtime, graph);
    runtime.shutdown();

    assert_eq!(results.get::<BuildStatus>(ids[0]), BuildStatus::Built);
}

#[test]
fn failed_prerequisite_causes_dependent_to_be_skipped() {
    // Deterministic regardless of keep_going: `downstream` can only be
    // scheduled after `bad` finishes, and it skips based on `bad`'s
    // stored status, not the global fail_flag race.
    for keep_going in [false, true] {
        let rules = vec![
            rule("bad", &[], &["false"], false),
            rule("downstream", &["bad"], &["true"], false),
        ];
        let goals = vec!["bad".to_string(), "downstream".to_string()];
        let (graph, ids) = planner::plan(&parsed(rules), &goals, keep_going).unwrap();
        let runtime = Runtime::new(2);
        let results = run_graph(&runtime, graph);
        runtime.shutdown();

        assert_eq!(results.get::<BuildStatus>(ids[0]), BuildStatus::Failed);
        assert_eq!(results.get::<BuildStatus>(ids[1]), BuildStatus::Skipped);
    }
}
