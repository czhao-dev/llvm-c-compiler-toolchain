use c_static_analyzer::analyzer::iter_c_files;
use c_static_analyzer::config::Config;

#[test]
fn default_excludes_skip_build_and_vcs_dirs() {
    let tmp = tempfile::tempdir().unwrap();
    std::fs::create_dir(tmp.path().join("src")).unwrap();
    std::fs::write(
        tmp.path().join("src").join("app.c"),
        "int main(void) { return 0; }\n",
    )
    .unwrap();
    let build_dir = tmp.path().join("build").join("obj");
    std::fs::create_dir_all(&build_dir).unwrap();
    std::fs::write(build_dir.join("generated.c"), "int x;\n").unwrap();

    let config = Config::default();
    let mut found: Vec<String> = iter_c_files(&[tmp.path().to_path_buf()], &config.exclude)
        .iter()
        .map(|p| p.file_name().unwrap().to_string_lossy().to_string())
        .collect();
    found.sort();

    assert_eq!(found, vec!["app.c".to_string()]);
}

#[test]
fn custom_exclude_pattern() {
    let tmp = tempfile::tempdir().unwrap();
    std::fs::write(tmp.path().join("keep.c"), "int x;\n").unwrap();
    std::fs::write(tmp.path().join("generated.c"), "int x;\n").unwrap();

    let mut found: Vec<String> =
        iter_c_files(&[tmp.path().to_path_buf()], &["*generated*".to_string()])
            .iter()
            .map(|p| p.file_name().unwrap().to_string_lossy().to_string())
            .collect();
    found.sort();

    assert_eq!(found, vec!["keep.c".to_string()]);
}

#[test]
fn discovers_header_files() {
    let tmp = tempfile::tempdir().unwrap();
    std::fs::write(tmp.path().join("lib.h"), "int add(int a, int b);\n").unwrap();
    std::fs::write(
        tmp.path().join("lib.c"),
        "int add(int a, int b) { return a + b; }\n",
    )
    .unwrap();

    let mut found: Vec<String> = iter_c_files(&[tmp.path().to_path_buf()], &[])
        .iter()
        .map(|p| p.file_name().unwrap().to_string_lossy().to_string())
        .collect();
    found.sort();

    assert_eq!(found, vec!["lib.c".to_string(), "lib.h".to_string()]);
}
