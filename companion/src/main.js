// Nutzt die globale Tauri-API (withGlobalTauri=true) -> kein Bundler noetig.
const { invoke } = window.__TAURI__.core;

// Externe Links (z. B. Treiber-Download) im System-Browser statt im App-Fenster
// oeffnen -- die Tauri-Webview navigiert target=_blank sonst nirgendwohin.
// Nutzt den plugin:opener-Befehl direkt ueber invoke (kein JS-Wrapper-Paket
// noetig, siehe tauri-plugin-opener in Cargo.toml + capabilities/default.json).
// Als Funktion, weil applyI18n() den Treiber-Hinweis (enthaelt <a>-Tags) per
// innerHTML neu schreibt -- die alten Listener gehen dabei verloren.
function attachExternalLinks() {
  document.querySelectorAll('a[target="_blank"]').forEach((a) => {
    a.addEventListener("click", (e) => {
      e.preventDefault();
      invoke("plugin:opener|open_url", { url: a.href });
    });
  });
}
attachExternalLinks();

// ---- Uebersetzungen (DE/EN), Muster gespiegelt an src/WebPortal.cpp (Firmware) ----
const T = {
  de: {
    clock: "Uhrzeit", customText: "Eigener Text", off: "Aus",
    textPh: "Text...", scroll: "Scrollen", show: "Anzeigen",
    countup: "Hochzählen", play: "Start", pause: "Pause", stop: "Stopp", extraLeds: "Zusatz-LEDs", on: "An",
    altBlinkBtn: "Wechselblinken",
    devSettings: "Geräte-Einstellungen", devHintPrefix: "Werden über die gewählte Verbindung gesendet:",
    dispName: "Anzeigename", hostname: "Hostname", ntpSync: "Zeit per NTP synchronisieren",
    ntpServer: "NTP-Server", setManual: "Uhrzeit manuell setzen", applyTimePc: "Zeit übernehmen (PC-Zeit)",
    load: "Laden", save: "Speichern",
    currentlyShown: "Aktuell angezeigt: ",
    realVolt: "Echte Spannung (Multimeter an OUT+/OUT-)", battVoltPh: "z.B. 4.08", calibrate: "Kalibrieren",
    brightness: "Helligkeit", statusLedBright: "Helligkeit Zusatz-LEDs", orientLabel: "Ausrichtung der Schrift", orient0: "Normal (0°)",
    orient180: "180° (auf dem Kopf)", scrollDirLabel: "Scrollrichtung", scrollLeft: "Links (Standard)",
    scrollRight: "Rechts",
    bootAnimLabel: "Animation beim Einschalten", bootOff: "Aus", bootScan: "Scan-Wipe",
    bootFill: "Pixel-Fill", bootProgress: "Fortschrittsbalken (WiFi-Verbindung)", bootPreviewBtn: "Vorschau",
    blinkSpeed: "Blinkgeschwindigkeit (manuell, ms je Zyklus)",
    autoWarn: "Countdown-Warnung (blinken)", warnMin: "Warnfenster (Min)",
    fastSecs: "Schnell-Blink (Sek, 0=aus)", altBlink: "Wechselblinken (abwechselnd)",
    activeLow: "LEDs invertiert (Active-Low)",
    wifiPassLabel: "Passwort", wifiPassPh: "(leer = offenes Netz)",
    statusBtn: "Status", saveRestart: "Speichern &amp; Neustart",
    mqttEnabledLabel: "MQTT aktiviert", host: "Host", mqttUser: "Benutzer",
    mqttPassPh: "(leer = unverändert)", mqttBase: "Basis-Topic",
    connection: "Verbindung", langLabel: "Sprache", wifiHttp: "WiFi (HTTP)", usbCable: "USB-Kabel",
    driverHint: 'Für USB wird der CH340-Treiber des D1 mini benötigt.\n      Windows: <a href="https://www.wch-ic.com/downloads/CH341SER_EXE.html" target="_blank" rel="noopener">Treiber von WCH</a>.\n      macOS: meist schon vorinstalliert, sonst\n      <a href="https://github.com/WCHSoftGroup/ch34xser_macos" target="_blank" rel="noopener">WCH-macOS-Treiber</a>.',
    autoCallLabel: 'Automatisch „In a Call" bei Mikrofonnutzung',
    errorPrefix: "Fehler: ", saved: "Gespeichert.", savedRestartHost: "Gespeichert. Neustart (Hostname)…",
    pickTime: "Bitte eine Uhrzeit wählen.", timeSetNoNtp: "Uhrzeit gesetzt (NTP deaktiviert).",
    pickVolts: "Bitte die mit dem Multimeter gemessene Spannung eintragen.",
    calibratedTo: "Kalibriert auf ", calibFailedNoMeasure: "Kalibrierung fehlgeschlagen (kurz warten, noch keine Messung?).",
    connectedTo: "Verbunden: ", apActive: "Setup-Hotspot aktiv", notConnected: "Nicht verbunden",
    statusUnavail: "Status nicht verfügbar", pickSsid: "Bitte eine SSID angeben.",
    savedRestartConnect: "Gespeichert. Display startet neu…",
    mqttDisabled: "Deaktiviert", mqttConnectedPrefix: "Verbunden (", mqttEnabledNotConn: "Aktiviert, nicht verbunden",
    noPortFound: "(kein Port gefunden)",
    warnActive: "(Countdown-Warnung aktiv)", deviceTime: "Gerätezeit: ",
    synced: "synchronisiert", notSynced: "nicht synchronisiert",
  },
  en: {
    clock: "Clock", customText: "Custom Text", off: "Off",
    textPh: "Text...", scroll: "Scroll", show: "Show",
    countup: "Count Up", play: "Start", pause: "Pause", stop: "Stop", extraLeds: "Extra LEDs", on: "On",
    altBlinkBtn: "Alternate Blink",
    devSettings: "Device Settings", devHintPrefix: "Sent over the selected connection:",
    dispName: "Display name", hostname: "Hostname", ntpSync: "Sync time via NTP",
    ntpServer: "NTP server", setManual: "Set time manually", applyTimePc: "Apply time (PC clock)",
    load: "Load", save: "Save",
    currentlyShown: "Currently shown: ",
    realVolt: "Actual voltage (multimeter at OUT+/OUT-)", battVoltPh: "e.g. 4.08", calibrate: "Calibrate",
    brightness: "Brightness", statusLedBright: "Extra LED brightness", orientLabel: "Text orientation", orient0: "Normal (0°)",
    orient180: "180° (upside down)", scrollDirLabel: "Scroll direction", scrollLeft: "Left (default)",
    scrollRight: "Right",
    bootAnimLabel: "Animation on power-up", bootOff: "Off", bootScan: "Scan wipe",
    bootFill: "Pixel fill", bootProgress: "Progress bar (WiFi connection)", bootPreviewBtn: "Preview",
    blinkSpeed: "Blink speed (manual, ms per cycle)",
    autoWarn: "Countdown warning (blink)", warnMin: "Warning window (min)",
    fastSecs: "Fast blink (sec, 0=off)", altBlink: "Alternate blink (take turns)",
    activeLow: "LEDs inverted (active-low)",
    wifiPassLabel: "Password", wifiPassPh: "(blank = open network)",
    statusBtn: "Status", saveRestart: "Save &amp; restart",
    mqttEnabledLabel: "MQTT enabled", host: "Host", mqttUser: "User",
    mqttPassPh: "(blank = unchanged)", mqttBase: "Base topic",
    connection: "Connection", langLabel: "Language", wifiHttp: "WiFi (HTTP)", usbCable: "USB cable",
    driverHint: 'USB requires the D1 mini\'s CH340 driver.\n      Windows: <a href="https://www.wch-ic.com/downloads/CH341SER_EXE.html" target="_blank" rel="noopener">driver from WCH</a>.\n      macOS: usually preinstalled, otherwise\n      <a href="https://github.com/WCHSoftGroup/ch34xser_macos" target="_blank" rel="noopener">WCH macOS driver</a>.',
    autoCallLabel: 'Automatically switch to "In a Call" when the microphone is in use',
    errorPrefix: "Error: ", saved: "Saved.", savedRestartHost: "Saved. Restarting (hostname changed)…",
    pickTime: "Please choose a time.", timeSetNoNtp: "Time set (NTP disabled).",
    pickVolts: "Please enter the voltage measured with the multimeter.",
    calibratedTo: "Calibrated to ", calibFailedNoMeasure: "Calibration failed (wait briefly, no measurement yet?).",
    connectedTo: "Connected: ", apActive: "Setup hotspot active", notConnected: "Not connected",
    statusUnavail: "Status unavailable", pickSsid: "Please enter an SSID.",
    savedRestartConnect: "Saved. Display is restarting…",
    mqttDisabled: "Disabled", mqttConnectedPrefix: "Connected (", mqttEnabledNotConn: "Enabled, not connected",
    noPortFound: "(no port found)",
    warnActive: "(countdown warning active)", deviceTime: "Device time: ",
    synced: "synchronized", notSynced: "not synchronized",
  },
};
let currentLang = "de";
function tr(k) { return (T[currentLang] || T.de)[k] || k; }
function applyI18n() {
  document.documentElement.lang = currentLang;
  document.querySelectorAll("[data-i18n]").forEach((e) => { e.innerHTML = tr(e.getAttribute("data-i18n")); });
  document.querySelectorAll("[data-i18n-ph]").forEach((e) => { e.placeholder = tr(e.getAttribute("data-i18n-ph")); });
  document.querySelectorAll("[data-i18n-title]").forEach((e) => { e.title = tr(e.getAttribute("data-i18n-title")); });
  attachExternalLinks();   // driverHint wurde per innerHTML neu geschrieben -> Listener neu binden
}
async function setLang(l) {
  currentLang = l;
  applyI18n();
  try { await invoke("set_language", { lang: l }); } catch (e) { /* Persistenz optional */ }
}
document.getElementById("lang").addEventListener("change", (e) => setLang(e.target.value));

// Preset-Wert -> angezeigter Text (fuer die Markierung des aktiven Status).
const PRESET_TEXT = { onair: "ON AIR", call: "IN A CALL", busy: "BUSY", brb: "BRB" };

// Markiert den Button, der zum aktuellen Anzeige-Zustand passt.
function highlight(st) {
  if (!st) return;
  const mode = st.mode || "", text = st.text || "";
  const isText = mode.indexOf("text") >= 0;
  const presets = Object.values(PRESET_TEXT);
  document.querySelectorAll("button[data-action],button[data-mode]").forEach((b) => {
    const a = b.dataset.action, v = b.dataset.value, m = b.dataset.mode;
    let on = false;
    if (a === "preset") on = (text === PRESET_TEXT[v]);
    else if (a === "clock") on = (mode === "clock");
    else if (a === "clear") on = (mode === "idle");
    else if (m === "text") on = (isText && !presets.includes(text));
    else if (m === "timer") on = (mode === "timer");
    b.classList.toggle("active", on);
  });
  highlightLeds(st);
  timerButtons(st);
}

// Zusatz-LEDs: aktiven Zustand-Button je LED hervorheben (manueller Zustand
// led1/led2, nicht die Countdown-Warnung-Vorschau) + Wechselblinken-Button +
// Warnhinweis, waehrend eine Countdown-Warnung laeuft.
function highlightLeds(st) {
  if (!st || st.led1 === undefined) return;   // Antwort ohne LED-Felder
  document.querySelectorAll("button[data-led]").forEach((b) => {
    const cur = b.dataset.led === "1" ? st.led1 : st.led2;
    b.classList.toggle("active", cur === b.dataset.s);
  });
  document.getElementById("btnAlt").classList.toggle("active", !!st.alt);
  document.getElementById("ledWarnHint").textContent = st.warn ? tr("warnActive") : "";
}

// Zusatz-LEDs schalten: ueber send_config (wie die Geraete-Einstellungen), NICHT
// send_command -- ein LED-Klick ist kein Anzeige-Status und darf last_manual
// (Basis fuer die Auto-Status-Wiederherstellung nach einem Anruf) nicht ueberschreiben.
function led(i, s) {
  cfg("led", `${i} ${s}`).then((res) => { try { highlightLeds(JSON.parse(res)); } catch (e) {} });
}
document.querySelectorAll("button[data-led]").forEach((b) => {
  b.addEventListener("click", () => led(b.dataset.led, b.dataset.s));
});
document.getElementById("btnAlt").addEventListener("click", () => {
  const on = document.getElementById("btnAlt").classList.contains("active");
  led("alt", on ? "off" : "on");
});

// Schickt einen Befehl an das Display ueber den in den Einstellungen gewaehlten
// Transportweg (HTTP oder USB). Die Antwort ist der JSON-Zustand -> direkt markieren.
async function send(action, value) {
  try {
    const res = await invoke("send_command", { action, value: String(value) });
    try { highlight(JSON.parse(res)); } catch (e) { /* keine JSON-Antwort */ }
  } catch (e) { /* Geraet evtl. nicht erreichbar */ }
}

// Pollt den aktuellen Zustand (nur bei sichtbarem Fenster), damit die Markierung
// auch Auto-Status (Mikrofon) und externe Aenderungen widerspiegelt.
async function refreshState() {
  if (document.hidden) return;
  try { highlight(JSON.parse(await invoke("send_config", { action: "state", value: "" }))); }
  catch (e) { /* Geraet evtl. nicht erreichbar */ }
}

// --- Modus-Panels (Eigener Text / Timer): nur bei Auswahl einblenden ---
// Blendet das Options-Panel des gewaehlten Modus ein, andere aus, und markiert
// den zugehoerigen Button. p === null schliesst alle Panels.
function setSel(p) {
  document.getElementById("textcfg").classList.toggle("hidden", p !== "text");
  document.getElementById("timercfg").classList.toggle("hidden", p !== "timer");
  document.querySelectorAll("button[data-mode]").forEach((b) =>
    b.classList.toggle("sel", b.dataset.mode === p));
}
document.querySelectorAll("button[data-mode]").forEach((btn) => {
  // Nur das Panel oeffnen (noch nichts senden) – wie in der Web-UI.
  btn.addEventListener("click", () => setSel(btn.dataset.mode));
});

// --- Status-Buttons (preset / clock / clear): senden + Panels schliessen ---
document.querySelectorAll("button[data-action]").forEach((btn) => {
  btn.addEventListener("click", () => { setSel(null); send(btn.dataset.action, btn.dataset.value); });
});

// Beim Start das Panel passend zum aktuellen Anzeige-Modus oeffnen (einmalig).
async function initSel() {
  try {
    const st = JSON.parse(await invoke("send_config", { action: "state", value: "" }));
    if (st.mode === "timer") setSel("timer");
    else if ((st.mode || "").indexOf("text") >= 0 && !Object.values(PRESET_TEXT).includes(st.text || "")) setSel("text");
  } catch (e) { /* Geraet evtl. nicht erreichbar */ }
}

setInterval(refreshState, 3000);
document.addEventListener("visibilitychange", () => { if (!document.hidden) refreshState(); });
refreshState();
initSel();

// --- Eigener Text ---
document.getElementById("sendText").addEventListener("click", () => {
  const text = document.getElementById("text").value;
  const scroll = document.getElementById("scroll").checked;
  send(scroll ? "scroll" : "text", text);
});

// --- Timer: Play (Start/Fortsetzen) / Pause / Stop -- Semantik gespiegelt an
// der Firmware-Web-UI (src/WebPortal.cpp): Play startet neu oder setzt fort
// (je nachdem, ob der Timer gerade pausiert ist), Hochzaehlen per Umschalter
// statt eines eigenen Buttons.
const minutes = () => Number(document.getElementById("minutes").value) * 60;
const timerDirEl = document.getElementById("timerDir");
let timerIsPaused = false;
function timerButtons(st) {
  if (!st) return;
  const running = st.mode === "timer";
  timerIsPaused = running && !!st.timerPaused;
  document.getElementById("timerPlay").disabled = running && !timerIsPaused;
  document.getElementById("timerPause").disabled = !running || timerIsPaused;
  document.getElementById("timerStop").disabled = !running;
  timerDirEl.disabled = running;
}
document.getElementById("timerPlay").addEventListener("click", () => {
  if (timerIsPaused) { send("timer", "resume"); return; }
  localStorage.setItem("timerMin", document.getElementById("minutes").value);
  localStorage.setItem("timerDir", timerDirEl.checked ? "1" : "0");
  send("timer", timerDirEl.checked ? "up" : minutes());
});
document.getElementById("timerPause").addEventListener("click", () => send("timer", "pause"));
document.getElementById("timerStop").addEventListener("click", () => send("timer", "stop"));
(function () {
  const tm = localStorage.getItem("timerMin"); if (tm) document.getElementById("minutes").value = tm;
  if (localStorage.getItem("timerDir") === "1") timerDirEl.checked = true;
})();

// --- Einstellungen (im Rust-Backend persistiert) ---
async function loadSettings() {
  const s = await invoke("get_settings");
  document.querySelector(`input[name=transport][value=${s.transport}]`).checked = true;
  document.getElementById("host").value = s.http_host;
  document.getElementById("autoCall").checked = s.auto_call;
  currentLang = s.language === "en" ? "en" : "de";
  document.getElementById("lang").value = currentLang;
  applyI18n();
  document.getElementById("devhost").textContent =
    s.transport === "serial" ? ("USB " + (s.serial_port || "(kein Port)")) : ("WiFi " + s.http_host);
  await refreshPorts(s.serial_port);
}

async function refreshPorts(selected) {
  const ports = await invoke("list_serial_ports");
  const sel = document.getElementById("port");
  sel.innerHTML = "";
  if (ports.length === 0) {
    sel.appendChild(new Option(tr("noPortFound"), ""));
  }
  ports.forEach((p) => sel.appendChild(new Option(p, p)));
  if (selected) sel.value = selected;
}

document.getElementById("refreshPorts").addEventListener("click", () => refreshPorts());

document.getElementById("saveSettings").addEventListener("click", async () => {
  const settings = {
    transport: document.querySelector("input[name=transport]:checked").value,
    http_host: document.getElementById("host").value || "pixelstatus.local",
    serial_port: document.getElementById("port").value,
    auto_call: document.getElementById("autoCall").checked,
    language: currentLang,   // sonst wuerde save_settings die per setLang() gesetzte Sprache ueberschreiben
  };
  await invoke("save_settings", { new: settings });
});

// --- Geräte-Einstellungen: dasselbe get*/cfg*-Protokoll über den gewählten
//     Transport (USB oder HTTP). Werte werden als URL-kodierter Query-String
//     übergeben; das Rust-Backend leitet ihn passend weiter. ---
const $ = (id) => document.getElementById(id);
const setMsg = (id, txt) => { $(id).textContent = txt; };
const enc = encodeURIComponent;

// action = get*/cfg*, query = "k1=v1&k2=v2" (bereits URL-kodiert) bzw. "".
async function cfg(action, query) {
  return await invoke("send_config", { action, value: query || "" });
}

// Lokale Browserzeit (PC-Zeit) im datetime-local-Format (YYYY-MM-DDTHH:mm) --
// der manuelle Zeit-Regler wird daraus vorbefuellt statt aus der (evtl.
// falschen oder abgelaufenen) Geraetezeit, dann reicht meist ein Klick auf
// "Zeit uebernehmen (PC-Zeit)".
function localNowStr() {
  const d = new Date();
  return new Date(d.getTime() - d.getTimezoneOffset() * 60000).toISOString().slice(0, 16);
}
// Blendet NTP-Server bzw. manuelle Zeit passend zum Schalter ein/aus.
function ntpToggle() {
  const on = $("ntpEnabled").checked;
  $("ntpSrvWrap").classList.toggle("hidden", !on);
  $("manualWrap").classList.toggle("hidden", on);
  if (!on) $("mTime").value = localNowStr();   // frisch bei jedem Umschalten auf manuell
}
$("ntpEnabled").addEventListener("change", ntpToggle);

async function loadSystem() {
  try {
    const c = JSON.parse(await cfg("getsys", ""));
    $("sysName").value = c.name; $("sysHost").value = c.hostname;
    $("ntpEnabled").checked = c.ntpEnabled; $("ntpServer").value = c.ntpServer;
    $("mTime").value = localNowStr();   // Browserzeit (PC-Zeit) vorbefuellen
    $("sysTime").textContent = tr("deviceTime") + (c.time || "").replace("T", " ")
      + (c.synced ? " (" + tr("synced") + ")" : " (" + tr("notSynced") + ")");
    ntpToggle();
    setMsg("sysMsg", "");
  } catch (e) { setMsg("sysMsg", tr("errorPrefix") + e); }
  await loadBattery();
}

// Akku-Kalibrierung (Teil des System-Bereichs) -- nur sichtbar, wenn das Geraet
// BATTERY_MONITOR_ENABLED hat (getbatt liefert dann enabled:true).
async function loadBattery() {
  try {
    const b = JSON.parse(await cfg("getbatt", ""));
    $("sysBattWrap").classList.toggle("hidden", !b.enabled);
    if (b.enabled) $("battNow").textContent = `${tr("currentlyShown")}${b.voltage.toFixed(2)} V (${b.percent} %)`;
  } catch (e) { $("sysBattWrap").classList.add("hidden"); }
}
$("battCal").addEventListener("click", async () => {
  const v = $("battVolts").value;
  if (!v) { setMsg("battMsg", tr("pickVolts")); return; }
  try {
    const r = JSON.parse(await cfg("cfgbattcal", `volts=${enc(v)}`));
    setMsg("battMsg", r.saved ? `${tr("calibratedTo")}${parseFloat(v).toFixed(2)} V.` : tr("calibFailedNoMeasure"));
    if (r.saved) loadBattery();
  } catch (e) { setMsg("battMsg", tr("errorPrefix") + e); }
});
async function saveSystem() {
  const en = $("ntpEnabled").checked;
  const q = `name=${enc($("sysName").value)}&host=${enc($("sysHost").value)}`
    + `&ntpEnabled=${en ? 1 : 0}&ntpServer=${enc($("ntpServer").value)}`;
  try {
    const r = JSON.parse(await cfg("cfgsys", q));
    setMsg("sysMsg", r.restart ? tr("savedRestartHost") : tr("saved"));
    if (!r.restart) setTimeout(loadSystem, 300);   // Zeitanzeige aktualisieren
  } catch (e) { setMsg("sysMsg", tr("errorPrefix") + e); }
}
// Anzeigename sofort uebernehmen (wie Helligkeit/Ausrichtung) -- Hostname/NTP
// bleiben ueber saveSystem() batched, da ein Hostnamenwechsel einen Neustart ausloest.
async function nameApply() {
  try { await cfg("cfgsys", `name=${enc($("sysName").value)}`); setMsg("sysMsg", tr("saved")); }
  catch (e) { setMsg("sysMsg", tr("errorPrefix") + e); }
}
$("sysName").addEventListener("change", nameApply);
// Manuelle Zeit sofort setzen (PC-Zeit des gewaehlten Feldes) und NTP deaktivieren,
// sonst ueberschreibt es der naechste Sync. datetime-local -> Epoch-Sekunden.
async function setManualTime() {
  const v = $("mTime").value;
  if (!v) { setMsg("sysMsg", tr("pickTime")); return; }
  const epoch = Math.floor(new Date(v).getTime() / 1000);
  try {
    await cfg("cfgsys", `ntpEnabled=0&settime=${epoch}`);
    $("ntpEnabled").checked = false; ntpToggle();
    setMsg("sysMsg", tr("timeSetNoNtp"));
    setTimeout(loadSystem, 300);
  } catch (e) { setMsg("sysMsg", tr("errorPrefix") + e); }
}
$("setTime").addEventListener("click", setManualTime);

async function loadLed() {
  try {
    const c = JSON.parse(await cfg("getled", ""));
    $("ledAutoWarn").checked = c.autoWarn; $("ledActiveLow").checked = c.activeLow; $("ledAlt").checked = c.alternate;
    $("ledWarn").value = Math.round(c.warnSecs / 60); $("ledFast").value = c.warnFastSecs;
    $("ledBlinkPeriod").value = c.blinkPeriod;
    $("statusLedBright").value = c.ledBrightness;
    const a = JSON.parse(await cfg("getappear", ""));
    $("ledC1").value = a.led1; $("ledC2").value = a.led2; $("ledCM").value = a.matrix;
    const st = JSON.parse(await cfg("state", ""));   // Helligkeit/Ausrichtung/Scrollrichtung aus dem Status
    if (st.brightness != null) $("ledBright").value = st.brightness;
    if (st.orientation != null) $("ledOrient").value = st.orientation;
    if (st.scrollReverse != null) $("ledScrollDir").value = st.scrollReverse ? "right" : "left";
    if (st.bootAnim != null) $("ledBootAnim").value = st.bootAnim;
    setMsg("ledMsg", "");
  } catch (e) { setMsg("ledMsg", tr("errorPrefix") + e); }
}
async function saveLed() {
  const q = `autoWarn=${$("ledAutoWarn").checked ? 1 : 0}&activeLow=${$("ledActiveLow").checked ? 1 : 0}`
    + `&alternate=${$("ledAlt").checked ? 1 : 0}`
    + `&warnSecs=${Math.max(1, $("ledWarn").value * 60)}&warnFastSecs=${Math.max(0, $("ledFast").value)}`
    + `&blinkPeriod=${Math.min(5000, Math.max(100, $("ledBlinkPeriod").value))}`
    + `&ledBrightness=${Math.min(15, Math.max(0, $("statusLedBright").value))}`
    + `&led1=${enc($("ledC1").value)}&led2=${enc($("ledC2").value)}&matrix=${enc($("ledCM").value)}`;
  try {
    await cfg("cfgled", q);
    await cfg("brightness", String($("ledBright").value));   // runCommand-Aktion, beide Transporte
    await cfg("orient", $("ledOrient").value);
    await cfg("scrolldir", $("ledScrollDir").value);
    await cfg("bootanim", $("ledBootAnim").value);
    setMsg("ledMsg", tr("saved"));
  } catch (e) { setMsg("ledMsg", tr("errorPrefix") + e); }
}
// Spielt die aktuell gewaehlte Einschalt-Animation sofort einmal ab (wie das
// Dropdown selbst -- setBootAnimation() in der Firmware persistiert nur bei
// Aenderung, spielt aber immer ab), ohne die uebrigen LED-Einstellungen
// mitzuspeichern.
async function previewBootAnim() {
  try { await cfg("bootanim", $("ledBootAnim").value); }
  catch (e) { setMsg("ledMsg", tr("errorPrefix") + e); }
}
$("ledBootAnimPreview").addEventListener("click", previewBootAnim);
// Helligkeit direkt bei Loslassen des Reglers uebernehmen (wie im Firmware-Web-UI),
// nicht erst beim "Speichern"-Klick des restlichen LED-Tabs.
$("ledBright").addEventListener("change", () => cfg("brightness", String($("ledBright").value)));
$("statusLedBright").addEventListener("change", () => cfg("ledbrightness", String($("statusLedBright").value)));
// Restliche LED-Tab-Felder ebenfalls sofort uebernehmen (wie im Firmware-Web-UI) --
// saveLed() liest bei jedem Aufruf ohnehin den kompletten aktuellen Formularstand,
// der "Speichern"-Button entfaellt damit fuer den ganzen Tab.
["ledAutoWarn", "ledWarn", "ledFast", "ledBlinkPeriod", "ledAlt", "ledActiveLow", "ledC1", "ledC2", "ledCM"]
  .forEach((id) => $(id).addEventListener("change", saveLed));

async function loadWifi() {
  try {
    const c = JSON.parse(await cfg("getwifi", ""));
    $("wifiStatus").textContent = c.connected
      ? `${tr("connectedTo")}${c.ssid} (${c.ip})`
      : (c.ap ? tr("apActive") : tr("notConnected"));
    if (c.ssid) $("wifiSsid").value = c.ssid;
  } catch (e) { $("wifiStatus").textContent = tr("statusUnavail"); }
}
async function saveWifi() {
  if (!$("wifiSsid").value) { setMsg("wifiMsg", tr("pickSsid")); return; }
  const q = `ssid=${enc($("wifiSsid").value)}&pass=${enc($("wifiPass").value)}`;
  try { await cfg("cfgwifi", q); setMsg("wifiMsg", tr("savedRestartConnect")); }
  catch (e) { setMsg("wifiMsg", tr("errorPrefix") + e); }
}

async function loadMqtt() {
  try {
    const c = JSON.parse(await cfg("getmqtt", ""));
    $("mqttEnabled").checked = c.enabled; $("mqttHost").value = c.host; $("mqttPort").value = c.port;
    $("mqttUser").value = c.user; $("mqttBase").value = c.base;
    $("mqttStatus").textContent = !c.enabled ? tr("mqttDisabled")
      : (c.connected ? `${tr("mqttConnectedPrefix")}${c.host}:${c.port})` : tr("mqttEnabledNotConn"));
  } catch (e) { $("mqttStatus").textContent = tr("statusUnavail"); }
}
async function saveMqtt() {
  const q = `enabled=${$("mqttEnabled").checked ? 1 : 0}&host=${enc($("mqttHost").value)}`
    + `&port=${enc($("mqttPort").value)}&user=${enc($("mqttUser").value)}`
    + `&pass=${enc($("mqttPass").value)}&base=${enc($("mqttBase").value)}`;
  try { await cfg("cfgmqtt", q); setMsg("mqttMsg", tr("savedRestartConnect")); }
  catch (e) { setMsg("mqttMsg", tr("errorPrefix") + e); }
}

const loaders = { system: loadSystem, led: loadLed, wifi: loadWifi, mqtt: loadMqtt };
const savers = { system: saveSystem, led: saveLed, wifi: saveWifi, mqtt: saveMqtt };
document.querySelectorAll("button[data-load]").forEach((b) => b.addEventListener("click", () => loaders[b.dataset.load]()));
document.querySelectorAll("button[data-save]").forEach((b) => b.addEventListener("click", () => savers[b.dataset.save]()));
// Beim Aufklappen einer Kategorie deren aktuelle Werte laden.
document.querySelectorAll("details.dev").forEach((d) => {
  d.addEventListener("toggle", () => {
    if (d.open) loaders[d.querySelector("button[data-load]").dataset.load]();
  });
});

loadSettings();
