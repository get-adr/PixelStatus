mod mic;
mod settings;
mod teams;
mod transport;

use settings::Settings;
use std::sync::Mutex;
use std::time::Duration;
use tauri::{
    menu::{CheckMenuItem, Menu, MenuItem, PredefinedMenuItem},
    tray::TrayIconBuilder,
    AppHandle, Manager, State,
};

// Geteilter App-Zustand: Einstellungen + zuletzt MANUELL gesetzter Befehl
// (damit der Auto-Status nach dem Auflegen den vorherigen Zustand wiederherstellt).
struct AppState {
    settings: Mutex<Settings>,
    last_manual: Mutex<(String, String)>,
    // Zuletzt gesendeter eigener Text (action = "text"|"scroll", value) fuer den
    // Tray-Schnellzugriff "Eigener Text". None = noch keiner in dieser Sitzung.
    last_text: Mutex<Option<(String, String)>>,
    // Tray-Status-Eintraege, um das Haekchen sofort (ohne Poll-Verzoegerung) zu setzen.
    status_items: Vec<(&'static str, CheckMenuItem<tauri::Wry>)>,
    // Fuer die Sprachumschaltung: Beschriftung per set_text() aktualisieren (kein
    // Menue-Rebuild noetig), analog zum Haekchen-Update der status_items.
    settings_item: MenuItem<tauri::Wry>,
    quit_item: MenuItem<tauri::Wry>,
    // Letzte vom Auto-Status-Watcher abgefragte Teams-Praesenz (Client-ID +
    // Ergebnis). Das Fenster zeigt diese an, statt Graph selbst ein zweites
    // Mal abzufragen.
    teams_last: Mutex<Option<TeamsPresenceCache>>,
}

// (Client-ID, Ergebnis der Praesenzabfrage: Ok((availability, activity)) | Err(text))
type TeamsPresenceCache = (String, Result<(String, String), String>);

#[tauri::command]
fn get_settings(state: State<AppState>) -> Settings {
    state.settings.lock().unwrap().clone()
}

#[tauri::command]
fn save_settings(new: Settings, state: State<AppState>, app: AppHandle) -> Result<(), String> {
    *state.settings.lock().unwrap() = new.clone();
    settings::save(&app, &new).map_err(|e| e.to_string())
}

#[tauri::command]
fn list_serial_ports() -> Vec<String> {
    transport::list_ports()
}

// Konfig-/Status-Befehl (get*/cfg*) an das Display – ueber den gewaehlten
// Transport (HTTP oder USB), damit alle Einstellungen auch per USB laufen.
// Anders als send_command wird last_manual NICHT gesetzt (Konfig ist kein Status).
// Mutex wird geklont und sofort freigegeben – nie ueber das await gehalten.
#[tauri::command]
async fn send_config(
    action: String,
    value: String,
    state: State<'_, AppState>,
) -> Result<String, String> {
    let s = { state.settings.lock().unwrap().clone() };
    transport::dispatch(&s, &action, &value).await
}

#[tauri::command]
async fn send_command(
    action: String,
    value: String,
    state: State<'_, AppState>,
) -> Result<String, String> {
    // Manuelle Befehle merken; Lock SOFORT freigeben (nicht ueber das await halten).
    let current = {
        *state.last_manual.lock().unwrap() = (action.clone(), value.clone());
        // Nicht-leeren eigenen Text zusaetzlich fuer den Tray-Schnellzugriff merken.
        if (action == "text" || action == "scroll") && !value.is_empty() {
            *state.last_text.lock().unwrap() = Some((action.clone(), value.clone()));
        }
        state.settings.lock().unwrap().clone()
    };
    transport::dispatch(&current, &action, &value).await
}

// Uebersetzte Beschriftung eines Tray-Eintrags. Nur die Eintraege, die tatsaechlich
// deutsche Woerter sind ("Uhrzeit"/"Eigener Text"/"Aus"/Einstellungen/Beenden);
// "On Air"/"In a Call"/"Busy"/"BRB" sind Presetnamen und bleiben in beiden Sprachen
// Englisch (siehe CLAUDE.md-Konvention fuer die Firmware-Presets).
fn tray_label(id: &str, lang: &str) -> Option<&'static str> {
    let en = lang == "en";
    match id {
        "free" => Some(if en { "Free" } else { "Frei" }),
        "clock" => Some(if en { "Clock" } else { "Uhrzeit" }),
        "text" => Some(if en { "Custom Text" } else { "Eigener Text" }),
        "clear" => Some(if en { "Off" } else { "Aus" }),
        "settings" => Some(if en {
            "Settings…"
        } else {
            "Einstellungen…"
        }),
        "quit" => Some(if en { "Quit" } else { "Beenden" }),
        _ => None,
    }
}

// Setzt die Tray-Beschriftungen auf die gewaehlte Sprache um -- ueber dieselben
// gespeicherten Menu-Item-Handles wie die Haekchen-Aktualisierung, kein Rebuild.
fn apply_tray_language(app: &AppHandle, lang: &str) {
    let st = app.state::<AppState>();
    for (id, item) in &st.status_items {
        if let Some(label) = tray_label(id, lang) {
            let _ = item.set_text(label);
        }
    }
    if let Some(label) = tray_label("settings", lang) {
        let _ = st.settings_item.set_text(label);
    }
    if let Some(label) = tray_label("quit", lang) {
        let _ = st.quit_item.set_text(label);
    }
}

#[tauri::command]
fn set_language(lang: String, state: State<AppState>, app: AppHandle) -> Result<(), String> {
    let s = {
        let mut guard = state.settings.lock().unwrap();
        guard.language = lang.clone();
        guard.clone()
    };
    settings::save(&app, &s).map_err(|e| e.to_string())?;
    apply_tray_language(&app, &lang);
    Ok(())
}

#[tauri::command]
async fn teams_start_login(
    client_id: String,
    tenant: String,
) -> Result<teams::DeviceLogin, String> {
    teams::start_login(&client_id, &tenant).await
}

#[tauri::command]
async fn teams_complete_login(
    login: teams::DeviceLogin,
    client_id: String,
    tenant: String,
    state: State<'_, AppState>,
    app: AppHandle,
) -> Result<teams::TeamsAccount, String> {
    let account = teams::complete_login(&client_id, &tenant, login).await?;
    let mut settings = state.settings.lock().unwrap().clone();
    settings.teams_client_id = client_id;
    settings.teams_tenant = tenant;
    settings.teams_account = account.username.clone();
    settings::save(&app, &settings).map_err(|e| e.to_string())?;
    *state.settings.lock().unwrap() = settings;
    Ok(account)
}

#[tauri::command]
fn teams_logout(client_id: String, state: State<AppState>, app: AppHandle) -> Result<(), String> {
    teams::logout(&client_id)?;
    let mut settings = state.settings.lock().unwrap().clone();
    settings.teams_account.clear();
    settings::save(&app, &settings).map_err(|e| e.to_string())?;
    *state.settings.lock().unwrap() = settings;
    Ok(())
}

// Browser-Login (Alternative zum Device-Code-Flow). Start legt den Loopback-
// Listener an und liefert die Anmelde-URL (den oeffnet das Frontend); Complete
// wartet auf den Callback, tauscht den Code ein und persistiert die Einstellungen.
#[tauri::command]
fn teams_browser_start(
    client_id: String,
    tenant: String,
) -> Result<teams::BrowserLoginStart, String> {
    teams::browser_start_login(&client_id, &tenant)
}

#[tauri::command]
async fn teams_browser_complete(
    state: State<'_, AppState>,
    app: AppHandle,
) -> Result<teams::TeamsAccount, String> {
    let (account, client_id, tenant) = teams::browser_complete_login().await?;
    let mut settings = state.settings.lock().unwrap().clone();
    settings.teams_client_id = client_id;
    settings.teams_tenant = tenant;
    settings.teams_account = account.username.clone();
    settings::save(&app, &settings).map_err(|e| e.to_string())?;
    *state.settings.lock().unwrap() = settings;
    Ok(account)
}

// Login-Status + aktuelle Teams-Praesenz fuer die Anzeige im Fenster (rein
// lesend). `display` ist der daraus abgeleitete Matrix-Status (z. B.
// "onair"/"call"), falls einer zuzuordnen ist. `cached` nutzt die zuletzt vom
// Auto-Status-Watcher abgefragte Praesenz (keine zusaetzliche Graph-Abfrage);
// liegt noch keine vor, wird live abgefragt.
#[derive(serde::Serialize)]
struct TeamsStatusView {
    logged_in: bool,
    account: String,
    availability: String,
    activity: String,
    display: String,
    error: Option<String>,
}

#[tauri::command]
async fn teams_status(
    client_id: String,
    tenant: String,
    cached: Option<bool>,
    state: State<'_, AppState>,
) -> Result<TeamsStatusView, String> {
    let last = if cached.unwrap_or(false) {
        state
            .teams_last
            .lock()
            .unwrap()
            .as_ref()
            .filter(|(id, _)| *id == client_id)
            .map(|(_, r)| r.clone())
    } else {
        None
    };
    let p = match last {
        Some(result) => teams::status_with(&client_id, result),
        None => teams::status(&client_id, &tenant).await,
    };
    let display = if p.availability.is_empty() {
        String::new()
    } else {
        map_teams_status(&p.availability, &p.activity)
            .unwrap_or("")
            .to_string()
    };
    // Name aus dem Token (Keyring) bevorzugen -- der ist der Login-Status direkt
    // zugeordnet; aeltere Eintraege ohne Name fallen auf die App-Einstellung zurueck.
    let account = if p.account.is_empty() {
        state.settings.lock().unwrap().teams_account.clone()
    } else {
        p.account
    };
    Ok(TeamsStatusView {
        logged_in: p.logged_in,
        account,
        availability: p.availability,
        activity: p.activity,
        display,
        error: p.error,
    })
}

// Tray-Menupunkt -> (action, value) fuer das Display.
fn menu_command(id: &str) -> Option<(&'static str, &'static str)> {
    match id {
        "onair" => Some(("preset", "onair")),
        "call" => Some(("preset", "call")),
        "busy" => Some(("preset", "busy")),
        "brb" => Some(("preset", "brb")),
        "free" => Some(("preset", "free")),
        "dnd" => Some(("preset", "dnd")),
        "away" => Some(("preset", "away")),
        "clock" => Some(("clock", "on")),
        "clear" => Some(("clear", "")),
        _ => None,
    }
}

// Aktueller Anzeige-Status (aus der JSON-Antwort) -> Tray-Item-Id, das markiert
// werden soll (oder None, wenn kein Status-Eintrag passt, z. B. eigener Text/Timer).
fn active_status_id(json: &str) -> Option<String> {
    let v: serde_json::Value = serde_json::from_str(json).ok()?;
    let mode = v.get("mode").and_then(|x| x.as_str()).unwrap_or("");
    let text = v.get("text").and_then(|x| x.as_str()).unwrap_or("");
    let id = match text {
        "ON AIR" => "onair",
        "IN A CALL" => "call",
        "BUSY" => "busy",
        "BRB" => "brb",
        "FREE" => "free",
        "DND" => "dnd",
        "AWAY" => "away",
        // Jeder andere Text (z. B. die IP nach dem Neustart) ist "Eigener Text".
        _ if mode.contains("text") => "text",
        _ => match mode {
            "clock" => "clock",
            "idle" => "clear",
            _ => return None,
        },
    };
    Some(id.to_string())
}

// Setzt die Häkchen der Status-Einträge passend zum aktuellen Gerätezustand.
// Eigener OS-Thread + block_on (Transport blockiert); Menü-Update auf dem Main-Thread.
fn refresh_tray_checks(app: AppHandle) {
    std::thread::spawn(move || {
        let settings = app.state::<AppState>().settings.lock().unwrap().clone();
        let active = tauri::async_runtime::block_on(transport::dispatch(&settings, "state", ""))
            .ok()
            .and_then(|r| active_status_id(&r));
        let app2 = app.clone();
        let _ = app.run_on_main_thread(move || {
            let st = app2.state::<AppState>();
            for (id, item) in &st.status_items {
                let _ = item.set_checked(active.as_deref() == Some(*id));
            }
        });
    });
}

// Variante B: pollt den Gerätezustand alle 5 s (auch bei verstecktem Fenster, da das
// Tray immer sichtbar ist) und markiert den passenden Status-Eintrag per Häkchen.
fn spawn_tray_status_poller(app: AppHandle) {
    std::thread::spawn(move || loop {
        std::thread::sleep(Duration::from_secs(5));
        refresh_tray_checks(app.clone());
    });
}

fn handle_menu_event(app: &AppHandle, id: &str) {
    match id {
        "settings" => {
            if let Some(w) = app.get_webview_window("main") {
                let _ = w.show();
                let _ = w.set_focus();
            }
        }
        // "Eigener Text": zuletzt gesendeten Text erneut anzeigen. Gibt es keinen,
        // das Fenster oeffnen (dort tippt man den Text ein).
        "text" => {
            let last = app.state::<AppState>().last_text.lock().unwrap().clone();
            match last {
                Some((action, value)) => {
                    let app = app.clone();
                    // Eigener OS-Thread + block_on wie bei den Presets (USB-Transport blockiert).
                    std::thread::spawn(move || {
                        let state = app.state::<AppState>();
                        let current = {
                            *state.last_manual.lock().unwrap() = (action.clone(), value.clone());
                            state.settings.lock().unwrap().clone()
                        };
                        let _ = tauri::async_runtime::block_on(transport::dispatch(
                            &current, &action, &value,
                        ));
                        refresh_tray_checks(app.clone());
                    });
                }
                None => {
                    if let Some(w) = app.get_webview_window("main") {
                        let _ = w.show();
                        let _ = w.set_focus();
                    }
                    // Auto-Toggle des CheckMenuItem wieder am echten Zustand ausrichten.
                    refresh_tray_checks(app.clone());
                }
            }
        }
        "quit" => app.exit(0),
        other => {
            if let Some((action, value)) = menu_command(other) {
                // Sofort den passenden Eintrag markieren (sonst kurz zwei Haken bis
                // zum naechsten Poll; laeuft hier bereits auf dem Main-Thread).
                {
                    let st = app.state::<AppState>();
                    for (id, item) in &st.status_items {
                        let _ = item.set_checked(*id == other);
                    }
                }
                let app = app.clone();
                // Eigener OS-Thread + block_on (wie der Auto-Status-Watcher): der
                // USB-Transport blockiert, das darf den Tauri-Async-Runtime nicht
                // treffen. tauri::async_runtime::spawn wuerde einen Worker blockieren.
                std::thread::spawn(move || {
                    let state = app.state::<AppState>();
                    let current = {
                        *state.last_manual.lock().unwrap() = (action.into(), value.into());
                        state.settings.lock().unwrap().clone()
                    };
                    let _ = tauri::async_runtime::block_on(transport::dispatch(
                        &current, action, value,
                    ));
                });
            }
        }
    }
}

#[derive(Default)]
struct TeamsAutoState {
    last_sent: String,
    cleared: bool,
}

impl TeamsAutoState {
    fn next_command(
        &mut self,
        presence: &Result<(String, String), String>,
    ) -> Option<(&'static str, &'static str)> {
        match presence {
            Err(_) => (!self.cleared).then_some(("clear", "")),
            Ok((availability, activity)) => {
                if availability.is_empty() || availability == "Offline" {
                    return if self.cleared {
                        None
                    } else {
                        Some(("clear", ""))
                    };
                }
                let key = format!("{availability}:{activity}");
                if key == self.last_sent {
                    return None;
                }
                match map_teams_status(availability, activity) {
                    Some(preset) => Some(("preset", preset)),
                    None => {
                        self.last_sent = key;
                        None
                    }
                }
            }
        }
    }

    fn command_succeeded(&mut self, presence: &Result<(String, String), String>) {
        if let Ok((availability, activity)) = presence {
            if availability.is_empty() || availability == "Offline" {
                self.last_sent.clear();
                self.cleared = true;
            } else {
                self.last_sent = format!("{availability}:{activity}");
                self.cleared = false;
            }
        } else {
            self.last_sent.clear();
            self.cleared = true;
        }
    }
}

// Pollt im Hintergrund die Mikrofonnutzung. Schaltet bei Aktivierung auf
// "In a Call" und stellt beim Auflegen den zuletzt manuell gesetzten Status wieder
// her. Laeuft als OS-Thread (block_on fuer den async Transport).
fn spawn_auto_status_watcher(app: AppHandle) {
    std::thread::spawn(move || {
        let mut last_in_use = false;
        let mut teams_state = TeamsAutoState::default();
        let mut last_source = String::new();
        loop {
            std::thread::sleep(Duration::from_secs(10));
            let state = app.state::<AppState>();
            let settings = state.settings.lock().unwrap().clone();

            let source = if settings.auto_source.is_empty() {
                if settings.auto_call {
                    "microphone"
                } else {
                    "off"
                }
            } else {
                settings.auto_source.as_str()
            };
            if source != last_source {
                teams_state = TeamsAutoState::default();
                last_source = source.to_string();
            }
            if source == "off" {
                last_in_use = false; // bei Deaktivierung Flanke zuruecksetzen
                *state.teams_last.lock().unwrap() = None;
                continue;
            }

            if source == "teams" {
                let presence = tauri::async_runtime::block_on(teams::presence(
                    &settings.teams_client_id,
                    &settings.teams_tenant,
                ));
                *state.teams_last.lock().unwrap() =
                    Some((settings.teams_client_id.clone(), presence.clone()));
                if let Some((action, value)) = teams_state.next_command(&presence) {
                    match tauri::async_runtime::block_on(transport::dispatch(
                        &settings, action, value,
                    )) {
                        Ok(_) => teams_state.command_succeeded(&presence),
                        Err(e) => {
                            eprintln!("Teams-Status konnte nicht ans Display gesendet werden: {e}")
                        }
                    }
                }
            } else {
                *state.teams_last.lock().unwrap() = None;
                let in_use = mic::mic_in_use();
                if in_use == last_in_use {
                    continue;
                }
                last_in_use = in_use;
                let (action, value) = if in_use {
                    ("preset".to_string(), "call".to_string())
                } else {
                    state.last_manual.lock().unwrap().clone()
                };
                let _ =
                    tauri::async_runtime::block_on(transport::dispatch(&settings, &action, &value));
            }
        }
    });
}

fn map_teams_status(availability: &str, activity: &str) -> Option<&'static str> {
    if availability.is_empty() || availability == "Offline" {
        return None;
    }
    if matches!(activity, "InACall" | "InAMeeting" | "InAConferenceCall") {
        return Some("call");
    }

    match availability {
        "Busy" => Some("busy"),
        "DoNotDisturb" => Some("dnd"),
        "BeRightBack" => Some("brb"),
        "Available" => Some("free"),
        "Away" => Some("away"),
        _ => None,
    }
}

#[cfg(test)]
mod teams_auto_tests {
    use super::TeamsAutoState;

    #[test]
    fn clears_on_error_once_and_restores_on_recovery() {
        let mut state = TeamsAutoState::default();
        let available = Ok(("Available".into(), "Available".into()));
        let error = Err("Graph nicht erreichbar".into());
        assert_eq!(state.next_command(&available), Some(("preset", "free")));
        state.command_succeeded(&available);
        assert_eq!(state.next_command(&error), Some(("clear", "")));
        // Fehlgeschlagener Display-Befehl wird erneut versucht.
        assert_eq!(state.next_command(&error), Some(("clear", "")));
        state.command_succeeded(&error);
        assert_eq!(state.next_command(&error), None);
        assert_eq!(state.next_command(&available), Some(("preset", "free")));
        state.command_succeeded(&available);
        assert_eq!(state.next_command(&available), None);
    }

    #[test]
    fn away_maps_to_away_and_unknown_presence_does_not_turn_off_display() {
        let mut state = TeamsAutoState::default();
        let away = Ok(("Away".into(), "Away".into()));
        assert_eq!(state.next_command(&away), Some(("preset", "away")));
        state.command_succeeded(&away);
        assert_eq!(state.next_command(&away), None);
        assert_eq!(
            state.next_command(&Ok(("PresenceUnknown".into(), "Unknown".into()))),
            None
        );
    }

    #[test]
    fn offline_clears_once_even_with_call_activity_and_recovers() {
        let mut state = TeamsAutoState::default();
        let available = Ok(("Available".into(), "Available".into()));
        let offline = Ok(("Offline".into(), "InACall".into()));
        assert_eq!(state.next_command(&available), Some(("preset", "free")));
        state.command_succeeded(&available);
        assert_eq!(state.next_command(&offline), Some(("clear", "")));
        assert_eq!(state.next_command(&offline), Some(("clear", "")));
        state.command_succeeded(&offline);
        assert_eq!(state.next_command(&offline), None);
        assert_eq!(state.next_command(&available), Some(("preset", "free")));
        state.command_succeeded(&available);
        assert_eq!(state.next_command(&available), None);
    }

    #[test]
    fn offline_after_error_clear_does_not_clear_again() {
        let mut state = TeamsAutoState::default();
        let error = Err("Graph nicht erreichbar".into());
        let offline = Ok(("Offline".into(), "Offline".into()));
        assert_eq!(state.next_command(&error), Some(("clear", "")));
        state.command_succeeded(&error);
        assert_eq!(state.next_command(&offline), None);
    }

    #[test]
    fn empty_graph_presence_clears_even_with_call_activity() {
        let mut state = TeamsAutoState::default();
        let empty = Ok((String::new(), "InACall".into()));
        assert_eq!(state.next_command(&empty), Some(("clear", "")));
        state.command_succeeded(&empty);
        assert_eq!(state.next_command(&empty), None);
        assert_eq!(
            state.next_command(&Ok(("Away".into(), "Away".into()))),
            Some(("preset", "away"))
        );
    }
}

// Haelt die Uhr des Displays ueber USB nachgestellt. Der ESP8266 hat keine RTC:
// ohne WiFi/NTP kennt er keine Zeit, und der Schiebeschalter kann das Board
// stromlos machen -> Reboot -> Zeit weg. Solange der USB-Transport aktiv ist,
// schicken wir daher sofort und danach periodisch die PC-Zeit (UTC-Epoch; die
// Firmware wendet ihre TZ selbst an). Ueber HTTP unnoetig -> das Geraet hat dann
// WiFi und synchronisiert per NTP selbst.
fn spawn_serial_time_sync(app: AppHandle) {
    std::thread::spawn(move || loop {
        let settings = app.state::<AppState>().settings.lock().unwrap().clone();
        if settings.transport == "serial" && !settings.serial_port.is_empty() {
            let epoch = std::time::SystemTime::now()
                .duration_since(std::time::UNIX_EPOCH)
                .map(|d| d.as_secs())
                .unwrap_or(0);
            let _ = tauri::async_runtime::block_on(transport::dispatch(
                &settings,
                "settime",
                &epoch.to_string(),
            ));
        }
        std::thread::sleep(Duration::from_secs(60));
    });
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .setup(|app| {
            // Reine Tray-App: kein Dock-Icon/Eintrag im Cmd+Tab-Umschalter auf macOS
            // (sonst NSApplicationActivationPolicyRegular, Standard fuer Tauri-Apps).
            #[cfg(target_os = "macos")]
            app.set_activation_policy(tauri::ActivationPolicy::Accessory);

            let handle = app.handle();

            // Status-Eintraege als CheckMenuItem (markierbar) -> zuerst erstellen,
            // damit sie in den AppState (fuer sofortiges Setzen) wandern koennen.
            let onair = CheckMenuItem::with_id(app, "onair", "On Air", true, false, None::<&str>)?;
            let call = CheckMenuItem::with_id(app, "call", "In a Call", true, false, None::<&str>)?;
            let busy = CheckMenuItem::with_id(app, "busy", "Busy", true, false, None::<&str>)?;
            let brb = CheckMenuItem::with_id(app, "brb", "BRB", true, false, None::<&str>)?;
            let free = CheckMenuItem::with_id(app, "free", "Frei", true, false, None::<&str>)?;
            let dnd = CheckMenuItem::with_id(app, "dnd", "DND", true, false, None::<&str>)?;
            let away = CheckMenuItem::with_id(app, "away", "Away", true, false, None::<&str>)?;
            let clock = CheckMenuItem::with_id(app, "clock", "Uhrzeit", true, false, None::<&str>)?;
            let text =
                CheckMenuItem::with_id(app, "text", "Eigener Text", true, false, None::<&str>)?;
            let clear = CheckMenuItem::with_id(app, "clear", "Aus", true, false, None::<&str>)?;
            let status_items: Vec<(&'static str, CheckMenuItem<tauri::Wry>)> = vec![
                ("onair", onair.clone()),
                ("call", call.clone()),
                ("busy", busy.clone()),
                ("brb", brb.clone()),
                ("free", free.clone()),
                ("dnd", dnd.clone()),
                ("away", away.clone()),
                ("clock", clock.clone()),
                ("text", text.clone()),
                ("clear", clear.clone()),
            ];

            // restliches Tray-Menu -- vor app.manage() erstellt, damit die Handles in
            // den AppState (fuer die Sprachumschaltung per set_text()) wandern koennen.
            let sep = PredefinedMenuItem::separator(app)?;
            let settings_item =
                MenuItem::with_id(app, "settings", "Einstellungen…", true, None::<&str>)?;
            let quit_item = MenuItem::with_id(app, "quit", "Beenden", true, None::<&str>)?;

            let loaded_settings = settings::load(handle);
            let initial_lang = loaded_settings.language.clone();

            app.manage(AppState {
                settings: Mutex::new(loaded_settings),
                last_manual: Mutex::new(("clear".into(), String::new())),
                last_text: Mutex::new(None),
                status_items,
                settings_item: settings_item.clone(),
                quit_item: quit_item.clone(),
                teams_last: Mutex::new(None),
            });
            apply_tray_language(handle, &initial_lang); // gespeicherte Sprache sofort anwenden

            spawn_auto_status_watcher(handle.clone());
            // Tray-Markierung des aktiven Status per Hintergrund-Polling (Variante B).
            spawn_tray_status_poller(handle.clone());
            // Uhrzeit des Displays ueber USB nachstellen (kein RTC im ESP8266).
            spawn_serial_time_sync(handle.clone());

            let menu = Menu::with_items(
                app,
                &[
                    &onair,
                    &call,
                    &busy,
                    &brb,
                    &free,
                    &dnd,
                    &away,
                    &clock,
                    &text,
                    &clear,
                    &sep,
                    &settings_item,
                    &quit_item,
                ],
            )?;

            TrayIconBuilder::new()
                .icon(app.default_window_icon().unwrap().clone())
                .menu(&menu)
                .on_menu_event(|app, event| handle_menu_event(app, event.id().as_ref()))
                .build(app)?;

            Ok(())
        })
        // Fenster nicht beenden, sondern verstecken (App lebt im Tray).
        .on_window_event(|window, event| {
            if let tauri::WindowEvent::CloseRequested { api, .. } = event {
                let _ = window.hide();
                api.prevent_close();
            }
        })
        .invoke_handler(tauri::generate_handler![
            get_settings,
            save_settings,
            list_serial_ports,
            send_command,
            send_config,
            set_language,
            teams_start_login,
            teams_complete_login,
            teams_logout,
            teams_browser_start,
            teams_browser_complete,
            teams_status
        ])
        .run(tauri::generate_context!())
        .expect("Fehler beim Start der Tauri-App");
}
