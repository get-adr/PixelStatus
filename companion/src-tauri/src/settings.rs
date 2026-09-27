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
    pub auto_call: bool, // automatisch "In a Call" bei Mikrofonnutzung
    #[serde(default)]
    pub auto_source: String, // "off", "microphone" oder "teams"
    #[serde(default)]
    pub teams_client_id: String,
    #[serde(default = "default_tenant")]
    pub teams_tenant: String,
    #[serde(default)]
    pub teams_account: String,
    #[serde(default = "default_login_method")]
    pub teams_login_method: String, // "browser" oder "device"
    #[serde(default = "default_language")]
    pub language: String, // UI-Sprache: "de" oder "en"
}

fn default_language() -> String {
    "de".into()
}

fn default_tenant() -> String {
    "organizations".into()
}

// Browser-Login (Authorization-Code mit Loopback) ist der Default, weil der
// Device-Code-Flow in vielen Tenants gesperrt ist; "device" bleibt als Option.
fn default_login_method() -> String {
    "browser".into()
}

impl Default for Settings {
    fn default() -> Self {
        Self {
            transport: "http".into(),
            http_host: "pixelstatus.local".into(),
            serial_port: String::new(),
            auto_call: false,
            auto_source: "off".into(),
            teams_client_id: String::new(),
            teams_tenant: default_tenant(),
            teams_account: String::new(),
            teams_login_method: default_login_method(),
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
