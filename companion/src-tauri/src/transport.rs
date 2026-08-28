use crate::settings::Settings;
use std::io::{Read, Write};
use std::sync::Mutex;
use std::time::Duration;

// Serialisiert den Zugriff auf den seriellen Port: Fenster-, Tray-, Auto-Status-
// und Tray-Poller-Thread oeffnen ihn sonst evtl. gleichzeitig ("port busy").
static SERIAL_LOCK: Mutex<()> = Mutex::new(());

// Verfuegbare serielle Ports auflisten – vorgefiltert auf plausible Geraete.
// Ungefiltert erscheinen sonst Bluetooth-Incoming, Debug-Console, Headsets und
// (unter macOS) je Geraet ein tty.*- UND ein cu.*-Eintrag. Wir zeigen daher nur
// echte USB-Serial-Ports (SerialPortType::UsbPort) und bevorzugen cu.* vor tty.*.
pub fn list_ports() -> Vec<String> {
    let ports = match serialport::available_ports() {
        Ok(p) => p,
        Err(_) => return Vec::new(),
    };
    // 1) Nur echte USB-Serial-Adapter (CH340/CP210x/FTDI am ESP). Das entfernt
    //    Bluetooth-Incoming-Port, Debug-Console und BT-Headsets zuverlaessig.
    let mut names: Vec<String> = ports
        .iter()
        .filter(|p| matches!(p.port_type, serialport::SerialPortType::UsbPort(_)))
        .map(|p| p.port_name.clone())
        .collect();
    // Fallback: Meldet der Treiber keinen USB-Typ, lieber die (entrauschte)
    // Vollliste zeigen als eine leere Auswahl.
    if names.is_empty() {
        names = ports
            .into_iter()
            .map(|p| p.port_name)
            .filter(|n| {
                let l = n.to_lowercase();
                !l.contains("bluetooth") && !l.contains("debug-console")
            })
            .collect();
    }
    // 2) macOS liefert je Geraet /dev/cu.X und /dev/tty.X – fuer die Steuerung
    //    ist cu.X korrekt. tty.X droppen, wenn das cu.X-Pendant existiert.
    let cu: std::collections::HashSet<String> = names
        .iter()
        .filter(|n| n.starts_with("/dev/cu."))
        .cloned()
        .collect();
    names.retain(|n| match n.strip_prefix("/dev/tty.") {
        Some(rest) => !cu.contains(&format!("/dev/cu.{rest}")),
        None => true,
    });
    names
}

// Befehl je nach gewaehltem Transportweg ausliefern.
pub async fn dispatch(settings: &Settings, action: &str, value: &str) -> Result<String, String> {
    match settings.transport.as_str() {
        "serial" => send_serial(&settings.serial_port, action, value),
        _ => send_http(&settings.http_host, action, value).await,
    }
}

// WiFi-Weg: POST http://<host>/api/cmd, action/value als Formular-Body.
// Bewusst POST statt GET (wie zuvor): "value" traegt bei cfgwifi/cfgmqtt WLAN-/
// MQTT-Passwoerter im Klartext -- als GET-Query wuerden die in Server-/Proxy-Logs
// landen. Die Firmware akzeptiert auf /api/cmd ohnehin jede HTTP-Methode
// (ESP8266WebServer::on() ohne Methodenangabe = HTTP_ANY) und liest Formular-
// Body und Query-String identisch aus, es ist also keine Firmware-Aenderung
// fuer Nicht-Passwort-Aktionen (Presets, Timer, Status-Polling, ...) noetig.
async fn send_http(host: &str, action: &str, value: &str) -> Result<String, String> {
    let url = format!("http://{host}/api/cmd");
    let client = reqwest::Client::builder()
        .timeout(Duration::from_secs(5))
        .build()
        .map_err(|e| e.to_string())?;
    let resp = client
        .post(&url)
        .form(&[("action", action), ("value", value)])
        .send()
        .await
        .map_err(|e| e.to_string())?;
    resp.text().await.map_err(|e| e.to_string())
}

// USB-Weg: Zeile "<action> <value>\n" senden, eine Antwortzeile lesen.
fn send_serial(port: &str, action: &str, value: &str) -> Result<String, String> {
    if port.is_empty() {
        return Err("Kein serieller Port gewaehlt".into());
    }
    // Nur ein Thread spricht gleichzeitig mit dem Port (Poisoning ignorieren).
    let _guard = SERIAL_LOCK.lock().unwrap_or_else(|e| e.into_inner());
    let mut sp = serialport::new(port, 115200)
        .timeout(Duration::from_millis(800))
        .open()
        .map_err(|e| e.to_string())?;

    let line = format!("{action} {value}\n");
    sp.write_all(line.as_bytes()).map_err(|e| e.to_string())?;

    // Bis zur Antwort-Zeile lesen (der Port teilt sich ggf. mit Debug-Logs, daher
    // die letzte JSON-Zeile "{…}" herausfiltern). Timeout ist unkritisch.
    let mut acc: Vec<u8> = Vec::new();
    let mut buf = [0u8; 256];
    let deadline = std::time::Instant::now() + Duration::from_millis(1200);
    while std::time::Instant::now() < deadline {
        match sp.read(&mut buf) {
            Ok(0) => {}
            Ok(n) => {
                acc.extend_from_slice(&buf[..n]);
                if acc.contains(&b'\n') {
                    break;
                }
            }
            Err(_) => {} // Timeout eines Lesevorgangs -> weiter bis Deadline
        }
    }
    let text = String::from_utf8_lossy(&acc);
    let json = text.lines().rev().find(|l| l.trim_start().starts_with('{'));
    Ok(json
        .map(|l| l.trim().to_string())
        .unwrap_or_else(|| text.trim().to_string()))
}
