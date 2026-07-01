use std::process::Command;
use std::time::Duration;

fn scratch_dir(tag: &str) -> std::path::PathBuf {
    let dir = std::env::temp_dir().join(format!("parallel-make-e2e-{tag}-{}", std::process::id()));
    let _ = std::fs::remove_dir_all(&dir);
    std::fs::create_dir_all(&dir).unwrap();
    dir
}

fn run_parallel_make(dir: &std::path::Path) -> std::process::Output {
    Command::new(env!("CARGO_BIN_EXE_parallel-make"))
        .current_dir(dir)
        .output()
        .expect("failed to run parallel-make binary")
}

#[test]
fn diamond_build_skips_recipes_when_up_to_date_then_rebuilds_after_touch() {
    let dir = scratch_dir("diamond");
    std::fs::write(dir.join("common.h"), "shared header").unwrap();
    std::fs::write(
        dir.join("Makefile"),
        "all: a.out b.out\n\
         a.out: common.h\n\
         \techo a >> build.log\n\
         \ttouch a.out\n\
         b.out: common.h\n\
         \techo b >> build.log\n\
         \ttouch b.out\n",
    )
    .unwrap();

    let first = run_parallel_make(&dir);
    assert!(
        first.status.success(),
        "first run failed: {}",
        String::from_utf8_lossy(&first.stderr)
    );
    assert!(dir.join("a.out").exists());
    assert!(dir.join("b.out").exists());
    let log = std::fs::read_to_string(dir.join("build.log")).unwrap();
    assert_eq!(
        log.lines().count(),
        2,
        "both recipes should run once: {log}"
    );

    let second = run_parallel_make(&dir);
    assert!(second.status.success());
    let log = std::fs::read_to_string(dir.join("build.log")).unwrap();
    assert_eq!(
        log.lines().count(),
        2,
        "an unchanged rebuild should not rerun recipes: {log}"
    );

    std::thread::sleep(Duration::from_millis(1100));
    std::fs::write(dir.join("common.h"), "shared header v2").unwrap();

    let third = run_parallel_make(&dir);
    assert!(third.status.success());
    let log = std::fs::read_to_string(dir.join("build.log")).unwrap();
    assert_eq!(
        log.lines().count(),
        4,
        "touching the shared prerequisite should rebuild both targets: {log}"
    );

    std::fs::remove_dir_all(&dir).unwrap();
}

#[test]
fn failing_recipe_exits_with_failure_status() {
    let dir = scratch_dir("failure");
    std::fs::write(dir.join("Makefile"), "all:\n\tfalse\n").unwrap();

    let output = run_parallel_make(&dir);
    assert!(!output.status.success());
    assert_eq!(output.status.code(), Some(1));

    std::fs::remove_dir_all(&dir).unwrap();
}

#[test]
fn missing_makefile_exits_with_usage_error() {
    let dir = scratch_dir("missing");

    let output = run_parallel_make(&dir);
    assert!(!output.status.success());
    assert_eq!(output.status.code(), Some(2));

    std::fs::remove_dir_all(&dir).unwrap();
}
