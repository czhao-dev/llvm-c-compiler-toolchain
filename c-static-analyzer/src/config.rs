use std::path::Path;

pub const CONFIG_FILENAME: &str = ".c-static-analyzer.toml";

#[derive(Debug, Clone)]
pub struct Config {
    pub exclude: Vec<String>,
    pub max_complexity: i64,
    pub max_nesting: i64,
    pub enabled_rules: Vec<String>,
}

impl Default for Config {
    fn default() -> Self {
        Self {
            exclude: vec![],
            max_complexity: 10,
            max_nesting: 4,
            enabled_rules: vec![],
        }
    }
}

impl Config {
    pub fn is_enabled(&self, rule_id: &str) -> bool {
        self.enabled_rules.is_empty() || self.enabled_rules.iter().any(|r| r == rule_id)
    }
}

#[derive(serde::Deserialize, Default)]
struct RawConfig {
    exclude: Option<Vec<String>>,
    max_complexity: Option<i64>,
    max_nesting: Option<i64>,
    enabled_rules: Option<Vec<String>>,
}

/// Loads the nearest `.c-static-analyzer.toml` above `start`.
pub fn load_config(start: &Path) -> Config {
    for directory in start.ancestors() {
        let candidate = directory.join(CONFIG_FILENAME);
        if !candidate.is_file() {
            continue;
        }
        let Ok(text) = std::fs::read_to_string(&candidate) else {
            return Config::default();
        };
        let Ok(raw) = toml::from_str::<RawConfig>(&text) else {
            return Config::default();
        };
        let mut config = Config::default();
        if let Some(v) = raw.exclude {
            config.exclude = v;
        }
        if let Some(v) = raw.max_complexity {
            config.max_complexity = v;
        }
        if let Some(v) = raw.max_nesting {
            config.max_nesting = v;
        }
        if let Some(v) = raw.enabled_rules {
            config.enabled_rules = v;
        }
        return config;
    }
    Config::default()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn default_config_enables_all_rules() {
        let config = Config::default();
        assert!(config.is_enabled("SA001"));
        assert!(config.is_enabled("SA005"));
    }

    #[test]
    fn enabled_rules_restricts_selection() {
        let config = Config {
            enabled_rules: vec!["SA001".to_string()],
            ..Config::default()
        };
        assert!(config.is_enabled("SA001"));
        assert!(!config.is_enabled("SA002"));
    }

    #[test]
    fn load_config_reads_dedicated_file() {
        let dir = tempfile::tempdir().unwrap();
        std::fs::write(
            dir.path().join(CONFIG_FILENAME),
            "max_complexity = 5\nexclude = [\"vendor/*\"]\n",
        )
        .unwrap();
        let config = load_config(dir.path());
        assert_eq!(config.max_complexity, 5);
        assert_eq!(config.exclude, vec!["vendor/*".to_string()]);
        assert_eq!(config.max_nesting, 4);
    }

    #[test]
    fn load_config_falls_back_to_default_when_missing() {
        let dir = tempfile::tempdir().unwrap();
        let config = load_config(dir.path());
        assert_eq!(config.max_complexity, 10);
    }
}
