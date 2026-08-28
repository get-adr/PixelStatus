# Pixel Status Companion

Tray-/Menüleisten-App (Windows + macOS) zum Steuern des ESP8266-Pixel-Status-Displays.
Gebaut mit **Tauri v2** (Rust-Backend + HTML/JS-Frontend). Steuert das Display
wahlweise über **WiFi (HTTP)** oder das **USB-Kabel (seriell)** – umschaltbar in
den Einstellungen.

## Voraussetzungen

- **Node.js** (vorhanden) – liefert die Tauri-CLI über npm.
- **Rust** – noch installieren:
  ```bash
  curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
  ```
- Plattform-Abhängigkeiten für Tauri v2 siehe https://tauri.app/start/prerequisites/
  (macOS: Xcode Command Line Tools; Windows: WebView2 + MSVC Build Tools).

## Icons (einmalig vor dem ersten Build)

Tauri bettet App-/Tray-Icons ein, daher müssen sie existieren. Aus einem
beliebigen quadratischen PNG generieren:

```bash
npm install
npm run tauri icon pfad/zu/icon.png   # erzeugt src-tauri/icons/*
```

## Entwickeln & Bauen

```bash
npm install
npm run tauri dev      # App im Entwicklungsmodus starten
npm run tauri build    # Installer bauen: macOS .dmg / Windows .exe (nsis)
```

## Installation vorgefertigter Builds (GitHub Actions)

Der Workflow `.github/workflows/companion-build.yml` baut bei jedem Push nach
`main` (mit Änderungen unter `companion/`) automatisch Installer für Windows
x64/ARM64 und macOS als Artifacts.

**macOS: „ist beschädigt und kann nicht geöffnet werden“** – kein echter
Schaden, sondern Gatekeeper: die App ist **nicht mit einer Apple-Developer-ID
signiert/notarisiert**, und der Browser markiert heruntergeladene Dateien mit
dem Quarantäne-Flag (`com.apple.quarantine`). Statt der eigentlich
zutreffenden Meldung „von nicht verifiziertem Entwickler“ zeigt macOS in
diesem Fall fälschlich „beschädigt“. Fix nach der Installation:

```bash
xattr -cr "/Applications/Pixel Status Companion.app"
```

Danach lässt sich die App normal öffnen. Muss nach jedem neuen Download eines
Builds wiederholt werden. Dauerhaft beheben ließe sich das nur durch Code-
Signing + Notarisierung (Apple-Developer-Programm, 99 $/Jahr, plus
Zertifikat-Secrets im CI-Workflow) – für den privaten Gebrauch nicht
eingerichtet.

## Bedienung

- Die App lebt in der Menüleiste/Tray. Das Menü bietet die Presets direkt
  (On Air, In a Call, Busy, BRB, Uhrzeit, Aus) sowie „Einstellungen…“.
- Das Fenster (über „Einstellungen…“) bietet zusätzlich freien Text, Timer,
  Helligkeit und die Verbindungseinstellungen. Fenster-Schließen versteckt nur
  (App bleibt im Tray); Beenden über das Tray-Menü.
- **Verbindung** in den Einstellungen: WiFi (Host, Standard `pixelstatus.local`)
  oder USB (seriellen Port wählen, ↻ aktualisiert die Liste). Wird im
  App-Config-Verzeichnis persistiert.
- **Auto-Status:** Checkbox „Automatisch ‚In a Call'…" in den Einstellungen. Die
  App pollt alle 2 s die Mikrofonnutzung und schaltet bei Aktivität auf
  „In a Call"; beim Auflegen wird der zuvor manuell gesetzte Status
  wiederhergestellt (sonst geleert).
  - macOS: über CoreAudio (`kAudioDevicePropertyDeviceIsRunningSomewhere`) —
    erkennt jede Mikrofonnutzung, **ohne** Mikrofon-Berechtigung anzufordern.
  - Windows: über die Registry (`CapabilityAccessManager\ConsentStore`).

## Aufbau

| Datei | Zweck |
|-------|-------|
| `src/` | Frontend (index.html, main.js, styles.css), nutzt globales `window.__TAURI__` |
| `src-tauri/src/lib.rs` | Tray-Menü, Tauri-Commands, App-State |
| `src-tauri/src/transport.rs` | HTTP- und USB-Seriell-Versand, Port-Liste |
| `src-tauri/src/settings.rs` | Laden/Speichern der Einstellungen als JSON |
| `src-tauri/src/mic.rs` | Mikrofon-Nutzungserkennung (macOS/Windows) für den Auto-Status |

Alle Befehle gehen am Ende auf den Firmware-Endpunkt `/api/cmd?action=&value=`
(HTTP) bzw. die serielle Zeile `<action> <value>` – dieselben Aktionen wie in
`../src/Commands.h`.
