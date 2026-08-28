use serde::{Deserialize, Serialize};
use std::fs;
use std::path::PathBuf;
use tauri::{AppHandle, Manager};

// In der App und (als JSON) im App-Config-Verzeichnis gehaltene Einstellungen.
// Wird sowohl vom Fenster (get/save_settings) als auch vom Tray-Menu gelesen.
#[derive(Clone, Serialize, Deserialize)]
pub struct Settings {
    pub transport: String,   // "http" oder "serial"
    pub http_host: String,   // z. B. "pixelstatus.local"
    pub serial_port: String, // z. B. "/dev/tty.usbserial-110" oder "COM3"
    #[serde(default)]
    pub auto_call: bool,     // automatisch "In a Call" bei Mikrofonnutzung
    #[serde(default = "default_language")]
    pub language: String,    // UI-Sprache: "de" oder "en"
}

fn default_language() -> String {
    "de".into()
}

impl Default for Settings {
    fn default() -> Self {
        Self {
            transport: "http".into(),
            http_host: "pixelstatus.local".into(),
            serial_port: String::new(),
            auto_call: false,
            language: default_language(),
        }
    }
}

fn config_path(app: &AppHandle) -> PathBuf {
    let dir = app
        .path()
        .app_config_dir()
        .expect("kein Config-Verzeichnis");
    let _ = fs::create_dir_all(&dir);
    dir.join("settings.json")
}

pub fn load(app: &AppHandle) -> Settings {
    match fs::read_to_string(config_path(app)) {
        Ok(json) => serde_json::from_str(&json).unwrap_or_default(),
        Err(_) => Settings::default(),
    }
}

pub fn save(app: &AppHandle, settings: &Settings) -> std::io::Result<()> {
    let json = serde_json::to_string_pretty(settings).expect("serialize settings");
    fs::write(config_path(app), json)
}
