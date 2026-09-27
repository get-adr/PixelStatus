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

### Abhängigkeiten und Dependabot

Die App wird ausschließlich für **Windows und macOS** gebaut (siehe Workflow
unten). Tauri zieht seine Linux-Abhängigkeiten (`gtk`, `glib`, `webkit2gtk`,
`libappindicator` …) trotzdem in `Cargo.lock`, sie werden aber nur für
Linux-Ziele überhaupt kompiliert:

```bash
cargo tree -e normal -i glib --target x86_64-unknown-linux-gnu   # Treffer: gtk 0.18 -> tao/muda/tray-icon
cargo tree -e normal -i glib --target aarch64-apple-darwin       # "nothing to print"
```

Eine Dependabot-Meldung zu einem dieser Crates betrifft daher keinen
ausgelieferten Build. Sie lässt sich zudem nicht durch ein Update auflösen:
Tauri 2.x hängt über `tao`/`muda`/`tray-icon` fest an `gtk` 0.18 (gtk3-rs), das
seinerseits `glib` 0.18 vorgibt — ein Sprung auf `glib` 0.20 setzt ein
Upstream-Update von Tauri voraus. Vor dem Schließen einer solchen Meldung beide
`cargo tree`-Aufrufe oben wiederholen, statt die Einschätzung fortzuschreiben.

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
- **Auto-Status:** In den Einstellungen lässt sich die automatische Statusquelle
  wählen. Bei Mikrofon-Nutzung pollt die App die Mikrofonnutzung und schaltet
  bei Aktivität auf
  „In a Call"; beim Auflegen wird der zuvor manuell gesetzte Status
  wiederhergestellt (sonst geleert).
  - macOS: über CoreAudio (`kAudioDevicePropertyDeviceIsRunningSomewhere`) —
    erkennt jede Mikrofonnutzung, **ohne** Mikrofon-Berechtigung anzufordern.
  - Windows: über die Registry (`CapabilityAccessManager\ConsentStore`).

### Microsoft-Teams-Präsenz

Als automatische Statusquelle kann „Microsoft Teams“ gewählt werden. Die App
verwendet Microsoft Graph (`GET /me/presence`) und bietet zwei Login-Methoden
(Einstellungen → „Login-Methode“):

- **Browser (empfohlen, Default):** Authorization-Code-Flow mit PKCE. Die App
  hört kurz auf `127.0.0.1` (zufälliger Port), öffnet den Systembrowser auf der
  Anmelde-URL und tauscht den zurückgelieferten Code gegen Tokens ein. Der
  Loopback-Redirect ist für Public Clients mit PKCE ohne Registrierung erlaubt
  (RFC 8252), dadurch ist kein fester Callback-Port im Voraus nötig.
- **Device-Code:** gerätecodebasierter Login (Code im Browser eingeben), der in
  vielen Tenants gesperrt ist — als Option für Tenants, die ihn erlauben.

Dafür muss eine Microsoft-Entra-App-Registrierung als Public Client mit
delegierter Berechtigung `Presence.Read` angelegt und deren Client-ID
eingetragen werden; als Tenant ist `organizations` für Arbeits-/Schulkonten
voreingestellt. Die Registrierung lässt sich einmalig mit dem Helper-Skript im
Repo anlegen (läuft selbst per Browser-Login):

```powershell
pwsh tools/New-PixelStatusAppRegistration.ps1 -InstallModules
```

Es legt die App an, setzt `Presence.Read` und gibt die Client-ID aus, die in
die Einstellungen eingetragen wird.

Access- und Refresh-Tokens werden nicht in `settings.json` gespeichert, sondern
im nativen Credential Store des Betriebssystems. `InACall`/`InAMeeting` werden
zu „In a Call“, `Busy`/`DoNotDisturb` zu „Busy“, `BeRightBack` zu „BRB“ und
`Available` zu „On Air“ abgebildet.

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
