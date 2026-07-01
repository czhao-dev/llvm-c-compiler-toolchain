use assert_cmd::Command;
use predicates::prelude::*;

#[test]
fn scan_clean_file_exits_zero() {
    let dir = tempfile::tempdir().unwrap();
    let file = dir.path().join("clean.c");
    std::fs::write(&file, "int add(int a, int b) {\n    return a + b;\n}\n").unwrap();

    Command::cargo_bin("c-static-analyzer")
        .unwrap()
        .args(["scan", file.to_str().unwrap(), "--no-config"])
        .assert()
        .code(0)
        .stdout(predicate::str::is_empty());
}

#[test]
fn scan_file_with_issues_exits_one() {
    let dir = tempfile::tempdir().unwrap();
    let file = dir.path().join("bad.c");
    std::fs::write(
        &file,
        "int classify(int x) {\n    if (x > 0) {\n        return 1;\n    }\n}\n",
    )
    .unwrap();

    Command::cargo_bin("c-static-analyzer")
        .unwrap()
        .args(["scan", file.to_str().unwrap(), "--no-config"])
        .assert()
        .code(1)
        .stdout(predicate::str::contains("SA004"))
        .stdout(predicate::str::contains(file.to_str().unwrap()));
}

#[test]
fn select_filters_rules() {
    let dir = tempfile::tempdir().unwrap();
    let file = dir.path().join("bad.c");
    std::fs::write(
        &file,
        "int classify(int x) {\n    if (x > 0) {\n        return 1;\n    }\n}\n",
    )
    .unwrap();

    Command::cargo_bin("c-static-analyzer")
        .unwrap()
        .args([
            "scan",
            file.to_str().unwrap(),
            "--no-config",
            "--select",
            "SA001",
        ])
        .assert()
        .code(0)
        .stdout(predicate::str::is_empty());
}

#[test]
fn missing_path_exits_two() {
    Command::cargo_bin("c-static-analyzer")
        .unwrap()
        .args(["scan", "/no/such/path.c", "--no-config"])
        .assert()
        .code(2);
}
