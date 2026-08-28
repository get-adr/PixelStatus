#include "WebPortal.h"
#include "Commands.h"
#include "LoopStats.h"
#include "Icons.h"
#include <ESP8266WiFi.h>
#include <LittleFS.h>
#include <time.h>

// Kompilierzeitpunkt der Firmware (fuer die Diagnose-Seite).
static const char BUILD_TS[] = __DATE__ " " __TIME__;

// Gemeinsames Stylesheet fuer alle Seiten (unter /style.css). Farben ueber
// CSS-Variablen; der Dark Mode folgt automatisch dem System-Theme.
static const char CSS[] PROGMEM = R"CSS(
:root{
--bg:#ffffff;--fg:#111827;--muted:#6b7280;--card:#f1f5f9;--border:#cbd5e1;
--ibg:#ffffff;--ifg:#111827;--btn:#2563eb;--btna:#1e40af;--grey:#4b5563;
--on:#059669;--ring:#6ee7b7;--sel:#93c5fd;
--led1:#ef4444;--led1d:#3f1414;--led2:#22c55e;--led2d:#123322}
/* Auto (System) - ausser der Nutzer erzwingt Light per Schalter */
@media (prefers-color-scheme:dark){:root:not([data-theme=light]){
--bg:#0f172a;--fg:#e5e7eb;--muted:#94a3b8;--card:#1e293b;--border:#334155;
--ibg:#111827;--ifg:#e5e7eb;--btn:#3b82f6;--btna:#2563eb;--grey:#334155;
--on:#10b981;--ring:#065f46;--sel:#60a5fa}}
/* Manuell erzwungen per Schalter (Startseite) */
:root[data-theme=dark]{
--bg:#0f172a;--fg:#e5e7eb;--muted:#94a3b8;--card:#1e293b;--border:#334155;
--ibg:#111827;--ifg:#e5e7eb;--btn:#3b82f6;--btna:#2563eb;--grey:#334155;
--on:#10b981;--ring:#065f46;--sel:#60a5fa}
body{font-family:system-ui,sans-serif;max-width:480px;margin:1.5rem auto;padding:0 1rem;background:var(--bg);color:var(--fg)}
.hd{display:flex;align-items:center;justify-content:space-between;gap:.5rem}
.icon{width:auto;padding:.4rem .6rem;font-size:1.2rem;line-height:1;background:var(--grey)}
h1{font-size:1.4rem}h2{font-size:1rem;margin-top:1.5rem}
a{color:var(--btn)}
.grid{display:grid;grid-template-columns:repeat(2,1fr);gap:.5rem}
button{padding:.7rem;font-size:1rem;border:0;border-radius:.5rem;background:var(--btn);color:#fff;cursor:pointer}
button:active{background:var(--btna)}
button.grey{background:var(--grey)}
button.on{background:var(--on);box-shadow:0 0 0 3px var(--ring)}
button.sel{outline:3px solid var(--sel);outline-offset:2px}
button:disabled{opacity:.35;cursor:default}
.tctl{display:flex;gap:.5rem}
.ticon{flex:1;font-size:1.3rem;line-height:1;padding:.6rem}
.wide{width:100%;box-sizing:border-box;margin-top:.5rem;background:var(--grey)}
.cfg{margin-top:1rem}.hidden{display:none}
.hv{color:var(--muted);font-weight:400;font-size:.9rem}
.disp{width:100%;height:auto;background:#0a0a0a;border-radius:.5rem;display:block;margin:.3rem 0 1rem;image-rendering:pixelated}
label.f{display:block;margin-top:.7rem;font-size:.9rem;color:var(--fg)}
input,select{padding:.5rem;box-sizing:border-box;background:var(--ibg);color:var(--ifg);border:1px solid var(--border);border-radius:.4rem}
label.f input,label.f select,select{width:100%;margin:.25rem 0}
input[type=checkbox]{width:auto}
input[type=range]{width:100%;padding:0}
/* Color-Picker: Swatch die ganze Flaeche fuellen lassen (sonst nur ein duenner Strich) */
input[type=color]{width:100%;height:2.6rem;padding:.2rem;cursor:pointer}
input[type=color]::-webkit-color-swatch-wrapper{padding:0}
input[type=color]::-webkit-color-swatch{border:1px solid var(--border);border-radius:.25rem}
input[type=color]::-moz-color-swatch{border:1px solid var(--border);border-radius:.25rem}
#t{width:100%}#min{width:6rem}
#status{margin:.5rem 0;padding:.6rem;border-radius:.4rem;background:var(--card);color:var(--fg)}
#state{margin-top:1rem;color:var(--muted);font-family:monospace;word-break:break-all}
#health{margin-top:.5rem;color:var(--muted);font-size:.8rem;line-height:1.5}
#msg{margin-top:1rem;color:var(--muted)}
.switch{display:flex;align-items:center;justify-content:space-between;margin-top:1rem}
.tgl{position:relative;display:inline-block;width:3.2rem;height:1.7rem;flex:none}
.tgl input{opacity:0;width:0;height:0;position:absolute}
.sl{position:absolute;inset:0;background:var(--border);border-radius:1rem;transition:.2s;cursor:pointer}
.sl:before{content:"";position:absolute;height:1.3rem;width:1.3rem;left:.2rem;top:.2rem;background:#fff;border-radius:50%;transition:.2s}
.tgl input:checked+.sl{background:var(--btn)}
.tgl input:checked+.sl:before{transform:translateX(1.5rem)}
.ledwrap{display:flex;gap:.4rem;align-items:stretch;margin:.4rem 0}
.ledrows{flex:1;display:flex;flex-direction:column;gap:.4rem;min-width:0}
.altbtn{flex:none;width:5.5rem}
.ledrow{display:flex;align-items:center;gap:.4rem}
.ledrow .name{flex:none;width:5.5rem;color:var(--fg);display:flex;align-items:center;gap:.4rem}
.ledrow button{flex:1}
/* Farbige Swatches: konfigurierbar (LED1/LED2) */
.swatch{width:.85rem;height:.85rem;border-radius:50%;flex:none;box-shadow:0 0 0 1px var(--border) inset}
.swatch.s1{background:var(--led1)}.swatch.s2{background:var(--led2)}
/* Live-Statuspunkte neben dem Titel */
.titlewrap{display:flex;align-items:center;gap:.6rem;min-width:0;flex:1}
.leds-hd{display:flex;gap:.45rem;align-items:center}
.dot{width:1.05rem;height:1.05rem;border-radius:50%;flex:none;background:var(--c-off);box-shadow:0 0 0 1px var(--border) inset;transition:background .15s,box-shadow .15s}
.dot[data-led="1"]{--c-on:var(--led1);--c-off:var(--led1d)}
.dot[data-led="2"]{--c-on:var(--led2);--c-off:var(--led2d)}
.dot.on{background:var(--c-on);box-shadow:0 0 8px 1px var(--c-on)}
.dot.blink{background:var(--c-on);box-shadow:0 0 8px 1px var(--c-on);animation:ledblink 1s steps(1,end) infinite}
.dot.fast{background:var(--c-on);box-shadow:0 0 8px 1px var(--c-on);animation:ledblink .3s steps(1,end) infinite}
@keyframes ledblink{50%{background:var(--c-off);box-shadow:0 0 0 1px var(--border) inset}}
.ledlegend{display:flex;gap:1.2rem;margin:.6rem 0;color:var(--fg)}
.ledlegend span{display:flex;align-items:center;gap:.4rem}
/* Akkuanzeige im Titelbereich */
.batt-hd{display:flex;align-items:center;gap:.3rem;flex:none;margin-left:auto}
.battIcon{position:relative;width:1.35rem;height:.78rem;border:1.5px solid var(--muted);border-radius:2px;box-sizing:border-box;flex:none;padding:1px}
.battIcon:after{content:"";position:absolute;right:-.3rem;top:.18rem;width:.22rem;height:.34rem;background:var(--muted);border-radius:0 1px 1px 0}
.battFill{display:block;height:100%;width:0;background:var(--on);border-radius:1px;transition:width .3s}
.battFill.low{background:#ef4444}
.battPct{font-size:.78rem;color:var(--muted)}
/* Registerkarten der Einstellungsseite */
.tabs{display:flex;gap:.3rem;margin:1rem 0 .5rem;border-bottom:1px solid var(--border)}
.tab{background:transparent;color:var(--muted);border:0;border-bottom:2px solid transparent;border-radius:0;padding:.5rem .9rem;font-size:1rem;cursor:pointer}
.tab:active{background:transparent}
.tab.active{color:var(--fg);border-bottom-color:var(--btn)}
)CSS";

// Gemeinsames Sprach-Skript fuer alle Seiten (unter /lang.js), analog zu /style.css.
// Enthaelt das Uebersetzungswoerterbuch T (de/en) sowie applyI18n()/setLang()/
// toggleLang(). Sprache liegt bewusst nur clientseitig in localStorage (gleiches
// Muster wie der Dark/Light-Umschalter) -> kein neuer LittleFS-Speicher, keine
// neue get*/cfg*-Aktion. Wird am Ende jeder Seite (nach dem HTML) eingebunden,
// damit alle [data-i18n]-Elemente beim Lauf bereits existieren.
static const char LANG_JS[] PROGMEM = R"JS(
const T={
 de:{
  themeTitle:'Hell/Dunkel', langTitle:'Sprache',
  led1Title:'LED 1 (Rot)', led2Title:'LED 2 (Gruen)',
  clock:'Uhrzeit', customText:'Eigener Text', timer:'Timer', off:'Aus',
  textPh:'Text...', scroll:'Scrollen', show:'Anzeigen',
  minutes:'Minuten', countup:'Hochzaehlen', stop:'Stopp',
  play:'Start', pause:'Pause',
  game:'Pixel Attack', gameUp:'Hoch (↑)', gameDown:'Runter (↓)', gameFire:'Feuer (Leertaste)', gameBomb:'Bombe (B)', reset:'Neustart',
  openMobile:'Touch-Steuerung (neuer Tab)', statScore:'Score', statLives:'Leben', statBombs:'Bomben', statTop:'Best',
  rotateHint:'Bitte Handy quer halten', closeTab:'Tab schließen',
  extraLeds:'Zusatz-LEDs', on:'An', blink:'Blink', altTitle:'Beide LEDs abwechselnd blinken',
  alternate:'Wechsel', settings:'Einstellungen', sysDiag:'System &amp; Diagnose',
  warnActive:'(Countdown-Warnung aktiv)',
  settingsTitle:'Einstellungen', tabSystem:'System', tabLed:'LED', tabWifi:'WiFi', tabMqtt:'MQTT',
  loading:'Wird geladen ...', dispName:'Anzeigename (Titel der Startseite)',
  hostname:'Hostname (erreichbar als &lt;name&gt;.local)',
  nameHint:'Der Anzeigename wird sofort übernommen. Eine Änderung des Hostnamens erfordert einen Neustart.',
  time:'Uhrzeit', deviceTime:'Gerätezeit: ', hostnamePrefix:'Hostname: ',
  ntpSync:'Zeit per NTP synchronisieren', ntpServer:'NTP-Server',
  setManual:'Uhrzeit manuell setzen', applyTime:'Zeit übernehmen',
  noNtpHint:'Ohne NTP (und ohne RTC) geht die Zeit bei jedem Neustart verloren.',
  save:'Speichern', battery:'Akku', battNow:'Aktuell angezeigt: -',
  battHint:'Mit einem Multimeter direkt an <code>OUT+</code>/<code>OUT-</code> (bzw. den Akku-Pluspol/Minuspol) messen und den Wert hier eintragen. Gleicht Bauteiltoleranzen im A0-Spannungsteiler aus, wirkt sofort, kein Neustart nötig.',
  realVolt:'Echte Spannung (Multimeter)', battVoltPh:'z.B. 4.08', calibrate:'Kalibrieren',
  blinkSpeed:'Blinkgeschwindigkeit (manuell, ms je Zyklus)', matrixBright:'Helligkeit Matrix',
  ledBright:'Helligkeit LEDs',
  orientLabel:'Ausrichtung der Schrift', orient0:'Normal (0°)', orient180:'180° (auf dem Kopf)',
  scrollDirLabel:'Scrollrichtung', scrollLeft:'Links (Standard)', scrollRight:'Rechts',
  bootAnimTitle:'Einschalt-Animation', bootAnimLabel:'Animation beim Einschalten',
  bootOff:'Aus', bootScan:'Scan-Wipe', bootFill:'Pixel-Fill',
  bootProgress:'Fortschrittsbalken (WiFi-Verbindung)', bootPreviewBtn:'Vorschau',
  autoWarn:'Countdown-Warnung (blinken)', warnMin:'Warnfenster in Minuten (ab hier blinken beide LEDs)',
  fastSecs:'Schnell-Blink in den letzten ... Sekunden (0 = aus)',
  altBlink:'Wechselblinken (LEDs abwechselnd)', activeLow:'LEDs invertiert (Active-Low)',
  webColors:'Farben (Web-Darstellung)',
  colorsHint:'Passe die Farben an die tatsächlich verbauten LEDs bzw. das Matrix-Panel an.',
  colorLed1:'Farbe LED 1', colorLed2:'Farbe LED 2', colorMatrix:'Farbe Matrix-Vorschau',
  selectNet:'Netz wählen', rescan:'Erneut scannen', connect:'Verbinden',
  wifiPass:'Passwort (leer = offenes Netz)', saveRestart:'Speichern &amp; Neustart',
  mqttEnabled:'MQTT aktiviert', mqttHost:'Broker-Host / IP', mqttHostPh:'z.B. 192.168.1.10',
  mqttUser:'Benutzer (optional)', mqttPass:'Passwort (leer = unveraendert)', mqttBase:'Basis-Topic',
  back:'&larr; Zurück zur Steuerung',
  statusUnavail:'Status nicht verfügbar.', saved:'Gespeichert.',
  savedRestartHost:'Gespeichert. Display startet neu (Hostname geändert)...',
  saveFailed:'Speichern fehlgeschlagen.', pickTime:'Bitte eine Uhrzeit wählen.',
  timeSetNoNtp:'Uhrzeit gesetzt (NTP deaktiviert).', setFailed:'Setzen fehlgeschlagen.',
  currentlyShown:'Aktuell angezeigt: ', pickVolts:'Bitte die mit dem Multimeter gemessene Spannung eintragen.',
  calibratedTo:'Kalibriert auf ', calibFailedNoMeasure:'Kalibrierung fehlgeschlagen (noch keine Messung vorhanden? kurz warten und erneut versuchen).',
  calibFailed:'Kalibrierung fehlgeschlagen.', warningActive:'Warnung aktiv',
  savedApplied:'Gespeichert und angewendet.', connectedTo:'Verbunden mit ',
  apActive:'Nicht verbunden - Setup-Hotspot aktiv.', notConnected:'Nicht verbunden.',
  scanning:'Scanne...', scanFailed:'Scan fehlgeschlagen', pickSsid:'Bitte eine SSID angeben.',
  savedRestartConnect:'Gespeichert. Display startet neu und verbindet sich...',
  mqttDisabled:'MQTT deaktiviert.', mqttEnabledNotConn:'Aktiviert, aber nicht verbunden.',
  savedRestart:'Gespeichert. Display startet neu...',
  healthTitle:'System &amp; Diagnose', heapFree:'Heap frei', cpuLoad:'CPU-Last (geschätzt)',
  frag:'Fragmentierung', wifiSignal:'WiFi-Signal', uptime:'Uptime', loopRate:'Loop-Rate',
  sketch:'Programm', fsCard:'Dateisystem', battCard:'Akku',
  freeHeapH:'Freier Heap', cpuLoadH:'CPU-Last', wifiSignalH:'WiFi-Signal', battVoltH:'Akkuspannung',
  timeH:'Zeit', network:'Netzwerk', fwChip:'Firmware &amp; Chip',
  synced:'synchronisiert', notSynced:'nicht synchronisiert',
  gateway:'Gateway', subnet:'Subnetz', dns:'DNS', mac:'MAC', hostnameRow:'Hostname',
  ssidRow:'SSID', bssid:'BSSID', wifiChannel:'WiFi-Kanal',
  apSsidRow:'AP-SSID', apClients:'Verbundene Geräte',
  sdk:'SDK', core:'Core', cpuClock:'CPU-Takt', chipId:'Chip-ID', build:'Build', flash:'Flash',
  configured:' (konfiguriert ', noConn:'Keine Verbindung zum Display...'
 },
 en:{
  themeTitle:'Light/Dark', langTitle:'Language',
  led1Title:'LED 1 (red)', led2Title:'LED 2 (green)',
  clock:'Clock', customText:'Custom Text', timer:'Timer', off:'Off',
  textPh:'Text...', scroll:'Scroll', show:'Show',
  minutes:'Minutes', countup:'Count Up', stop:'Stop',
  play:'Start', pause:'Pause',
  game:'Pixel Attack', gameUp:'Up (↑)', gameDown:'Down (↓)', gameFire:'Fire (Space)', gameBomb:'Bomb (B)', reset:'Reset',
  openMobile:'Touch controls (new tab)', statScore:'Score', statLives:'Lives', statBombs:'Bombs', statTop:'Best',
  rotateHint:'Please rotate your phone sideways', closeTab:'Close tab',
  extraLeds:'Extra LEDs', on:'On', blink:'Blink', altTitle:'Blink both LEDs alternately',
  alternate:'Alternate', settings:'Settings', sysDiag:'System &amp; Diagnostics',
  warnActive:'(countdown warning active)',
  settingsTitle:'Settings', tabSystem:'System', tabLed:'LED', tabWifi:'WiFi', tabMqtt:'MQTT',
  loading:'Loading ...', dispName:'Display name (home page title)',
  hostname:'Hostname (reachable as &lt;name&gt;.local)',
  nameHint:'The display name is applied immediately. Changing the hostname requires a restart.',
  time:'Time', deviceTime:'Device time: ', hostnamePrefix:'Hostname: ',
  ntpSync:'Sync time via NTP', ntpServer:'NTP server',
  setManual:'Set time manually', applyTime:'Apply time',
  noNtpHint:'Without NTP (and without an RTC) the time is lost on every restart.',
  save:'Save', battery:'Battery', battNow:'Currently shown: -',
  battHint:'Measure directly at <code>OUT+</code>/<code>OUT-</code> (or the battery plus/minus terminals) with a multimeter and enter the value here. Compensates for component tolerances in the A0 voltage divider, applies immediately, no restart needed.',
  realVolt:'Actual voltage (multimeter)', battVoltPh:'e.g. 4.08', calibrate:'Calibrate',
  blinkSpeed:'Blink speed (manual, ms per cycle)', matrixBright:'Matrix brightness',
  ledBright:'LED brightness',
  orientLabel:'Text orientation', orient0:'Normal (0°)', orient180:'180° (upside down)',
  scrollDirLabel:'Scroll direction', scrollLeft:'Left (default)', scrollRight:'Right',
  bootAnimTitle:'Startup animation', bootAnimLabel:'Animation on power-up',
  bootOff:'Off', bootScan:'Scan wipe', bootFill:'Pixel fill',
  bootProgress:'Progress bar (WiFi connection)', bootPreviewBtn:'Preview',
  autoWarn:'Countdown warning (blink)', warnMin:'Warning window in minutes (both LEDs blink from here)',
  fastSecs:'Fast blink in the last ... seconds (0 = off)',
  altBlink:'Alternate blink (LEDs take turns)', activeLow:'LEDs inverted (active-low)',
  webColors:'Colors (web display)',
  colorsHint:'Adjust the colors to match the LEDs/matrix panel actually installed.',
  colorLed1:'Color LED 1', colorLed2:'Color LED 2', colorMatrix:'Color matrix preview',
  selectNet:'Select network', rescan:'Scan again', connect:'Connect',
  wifiPass:'Password (blank = open network)', saveRestart:'Save &amp; restart',
  mqttEnabled:'MQTT enabled', mqttHost:'Broker host / IP', mqttHostPh:'e.g. 192.168.1.10',
  mqttUser:'User (optional)', mqttPass:'Password (blank = unchanged)', mqttBase:'Base topic',
  back:'&larr; Back to control',
  statusUnavail:'Status unavailable.', saved:'Saved.',
  savedRestartHost:'Saved. Display is restarting (hostname changed)...',
  saveFailed:'Save failed.', pickTime:'Please choose a time.',
  timeSetNoNtp:'Time set (NTP disabled).', setFailed:'Setting failed.',
  currentlyShown:'Currently shown: ', pickVolts:'Please enter the voltage measured with the multimeter.',
  calibratedTo:'Calibrated to ', calibFailedNoMeasure:'Calibration failed (no measurement yet? wait briefly and try again).',
  calibFailed:'Calibration failed.', warningActive:'Warning active',
  savedApplied:'Saved and applied.', connectedTo:'Connected to ',
  apActive:'Not connected - setup hotspot active.', notConnected:'Not connected.',
  scanning:'Scanning...', scanFailed:'Scan failed', pickSsid:'Please enter an SSID.',
  savedRestartConnect:'Saved. Display is restarting and connecting...',
  mqttDisabled:'MQTT disabled.', mqttEnabledNotConn:'Enabled, but not connected.',
  savedRestart:'Saved. Display is restarting...',
  healthTitle:'System &amp; Diagnostics', heapFree:'Free heap', cpuLoad:'CPU load (estimated)',
  frag:'Fragmentation', wifiSignal:'WiFi signal', uptime:'Uptime', loopRate:'Loop rate',
  sketch:'Sketch', fsCard:'Filesystem', battCard:'Battery',
  freeHeapH:'Free heap', cpuLoadH:'CPU load', wifiSignalH:'WiFi signal', battVoltH:'Battery voltage',
  timeH:'Time', network:'Network', fwChip:'Firmware &amp; Chip',
  synced:'synchronized', notSynced:'not synchronized',
  gateway:'Gateway', subnet:'Subnet', dns:'DNS', mac:'MAC', hostnameRow:'Hostname',
  ssidRow:'SSID', bssid:'BSSID', wifiChannel:'WiFi channel',
  apSsidRow:'AP SSID', apClients:'Connected devices',
  sdk:'SDK', core:'Core', cpuClock:'CPU clock', chipId:'Chip ID', build:'Build', flash:'Flash',
  configured:' (configured ', noConn:'No connection to the display...'
 }
};
function curLang(){ return localStorage.getItem('lang')==='en' ? 'en' : 'de'; }
// Heisst bewusst nicht "t": die Startseite hat ein Eingabefeld id="t" (Eigener
// Text), das per Browser-DOM-Autoglobal sonst mit einer gleichnamigen Funktion
// kollidieren wuerde (t.value in sendText()).
function tr(k){ return (T[curLang()]||T.de)[k] || k; }
function applyI18n(){
 document.documentElement.lang = curLang();
 document.querySelectorAll('[data-i18n]').forEach(function(e){ e.innerHTML = tr(e.getAttribute('data-i18n')); });
 document.querySelectorAll('[data-i18n-ph]').forEach(function(e){ e.placeholder = tr(e.getAttribute('data-i18n-ph')); });
 document.querySelectorAll('[data-i18n-title]').forEach(function(e){ e.title = tr(e.getAttribute('data-i18n-title')); });
 var lg = document.getElementById('lg'); if (lg) lg.textContent = curLang().toUpperCase();
}
function setLang(l){ localStorage.setItem('lang', l); applyI18n(); if (typeof onLangChange === 'function') onLangChange(); }
function toggleLang(){ setLang(curLang() === 'de' ? 'en' : 'de'); }
applyI18n();
)JS";

// Bedienseite. Im Flash (PROGMEM) gehalten, um RAM zu sparen.
static const char PAGE[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Pixel Status</title><link rel="stylesheet" href="/style.css">
<link rel="icon" href="/favicon.ico" sizes="any">
<link rel="icon" type="image/png" sizes="32x32" href="/favicon.png">
<link rel="apple-touch-icon" href="/apple-touch-icon.png">
<link rel="manifest" href="/manifest.json">
<meta name="theme-color" content="#0f172a">
<script>(function(){var t=localStorage.getItem('theme');if(t)document.documentElement.setAttribute('data-theme',t);})();</script>
</head><body>
<div class="hd">
 <div class="titlewrap"><h1 id="ttl">Pixel Status</h1>
  <span class="leds-hd">
   <span class="dot" data-led="1" data-i18n-title="led1Title" title="LED 1 (Rot)"></span>
   <span class="dot" data-led="2" data-i18n-title="led2Title" title="LED 2 (Gruen)"></span>
  </span>
  <span class="batt-hd" id="battHd" style="display:none">
   <span class="battIcon"><span class="battFill" id="battFill"></span></span>
   <span class="battPct" id="battPct"></span>
  </span></div>
 <div style="display:flex;gap:.4rem">
 <button id="lg" class="icon" onclick="toggleLang()" data-i18n-title="langTitle" title="Sprache">DE</button>
 <button id="tt" class="icon" onclick="toggleTheme()" data-i18n-title="themeTitle" title="Hell/Dunkel">&#9790;</button>
 </div></div>
<canvas id="disp" class="disp" width="384" height="96" title="Live-Vorschau"></canvas>
<div class="grid">
 <button data-txt="ON AIR" onclick="preset('onair')">On Air</button>
 <button data-txt="IN A CALL" onclick="preset('call')">In a Call</button>
 <button data-txt="BUSY" onclick="preset('busy')">Busy</button>
 <button data-txt="BRB" onclick="preset('brb')">BRB</button>
 <button data-mode="clock" onclick="clock()" data-i18n="clock">Uhrzeit</button>
 <button data-mode="text" onclick="choose('text')" data-i18n="customText">Eigener Text</button>
 <button data-mode="timer" onclick="choose('timer')" data-i18n="timer">Timer</button>
 <button data-mode="game" onclick="choose('game')" data-i18n="game">Pixel Attack</button>
 <button data-mode="idle" onclick="preset('off')" class="grey" data-i18n="off">Aus</button>
</div>
<div id="textcfg" class="cfg hidden">
 <h2 data-i18n="customText">Eigener Text</h2>
 <input type="text" id="t" data-i18n-ph="textPh" placeholder="Text...">
 <div class="switch"><span data-i18n="scroll">Scrollen</span>
  <label class="tgl"><input type="checkbox" id="s"><span class="sl"></span></label></div>
 <p><button onclick="sendText()" data-i18n="show">Anzeigen</button></p>
</div>
<div id="timercfg" class="cfg hidden">
 <h2 data-i18n="timer">Timer</h2>
 <input type="number" id="min" value="5" min="0"> <span data-i18n="minutes">Minuten</span>
 <div class="switch"><span data-i18n="countup">Hochzaehlen</span>
  <label class="tgl"><input type="checkbox" id="tdir"><span class="sl"></span></label></div>
 <p class="tctl">
  <button id="tPlay" class="ticon" onclick="timerPlay()" data-i18n-title="play" title="Start">&#9654;</button>
  <button id="tPause" class="ticon" onclick="timerPause()" data-i18n-title="pause" title="Pause">&#9208;</button>
  <button id="tStop" class="ticon grey" onclick="stopTimer()" data-i18n-title="stop" title="Stopp">&#9209;</button>
 </p>
</div>
<div id="gamecfg" class="cfg hidden">
 <h2 data-i18n="game">Pixel Attack</h2>
 <p class="hv" id="gStats">Score: 0 &middot; Leben: 3 &middot; Bomben: 2 &middot; Best: 0</p>
 <p class="tctl">
  <button id="gUp" class="ticon" onclick="gameCmd('up')" data-i18n-title="gameUp" title="Hoch (↑)">&#9650;</button>
  <button id="gDown" class="ticon" onclick="gameCmd('down')" data-i18n-title="gameDown" title="Runter (↓)">&#9660;</button>
  <button id="gFire" class="ticon" onclick="gameCmd('fire')" data-i18n-title="gameFire" title="Feuer (Leertaste)">&#9679;</button>
  <button id="gBomb" class="ticon grey" onclick="gameCmd('bomb')" data-i18n-title="gameBomb" title="Bombe (B)">&#10033;</button>
 </p>
 <p class="tctl">
  <button id="gStart" class="ticon" onclick="gameCmd('start')" data-i18n-title="play" title="Start">&#9654;</button>
  <button id="gReset" class="ticon grey" onclick="gameCmd('start')" data-i18n-title="reset" title="Neustart">&#8635;</button>
 </p>
 <button class="wide" onclick="window.open('/play','_blank')" data-i18n="openMobile">Touch-Steuerung (neuer Tab)</button>
</div>
<h2><span data-i18n="extraLeds">Zusatz-LEDs</span> <span id="ledwarn" class="hv"></span></h2>
<div class="ledwrap">
 <div class="ledrows">
  <div class="ledrow"><span class="name"><span class="swatch s1"></span>LED 1</span>
   <button data-led="1" data-s="off" onclick="led(1,'off')" class="grey" data-i18n="off">Aus</button>
   <button data-led="1" data-s="on" onclick="led(1,'on')" data-i18n="on">An</button>
   <button data-led="1" data-s="blink" onclick="led(1,'blink')" data-i18n="blink">Blink</button></div>
  <div class="ledrow"><span class="name"><span class="swatch s2"></span>LED 2</span>
   <button data-led="2" data-s="off" onclick="led(2,'off')" class="grey" data-i18n="off">Aus</button>
   <button data-led="2" data-s="on" onclick="led(2,'on')" data-i18n="on">An</button>
   <button data-led="2" data-s="blink" onclick="led(2,'blink')" data-i18n="blink">Blink</button></div>
 </div>
 <button class="altbtn" id="btnalt" onclick="ledAlt()" data-i18n-title="altTitle" title="Beide LEDs abwechselnd blinken" data-i18n="alternate">Wechsel</button>
</div>
<button class="wide" onclick="location.href='/settings'" data-i18n="settings">Einstellungen</button>
<button class="wide" onclick="location.href='/health'" data-i18n="sysDiag">System &amp; Diagnose</button>
<script src="/lang.js"></script>
<script>
// ---- Theme-Schalter (ueberschreibt die automatische System-Erkennung) ----
function darkNow(){
 const t=document.documentElement.getAttribute('data-theme');
 return t ? t==='dark' : matchMedia('(prefers-color-scheme:dark)').matches;
}
function themeIcon(){ document.getElementById('tt').innerHTML = darkNow()?'&#9728;':'&#9790;'; }
function toggleTheme(){
 const d=!darkNow();
 document.documentElement.setAttribute('data-theme', d?'dark':'light');
 localStorage.setItem('theme', d?'dark':'light');
 themeIcon();
}
themeIcon();

const PRESETS=['ON AIR','IN A CALL','BUSY','BRB'];
let sel=null;   // aktuell gewaehltes Panel: 'text' | 'timer' | null

// Blendet das Config-Panel des gewaehlten Modus ein, andere aus.
function setSel(p){
 sel=p;
 document.getElementById('textcfg').classList.toggle('hidden', p!=='text');
 document.getElementById('timercfg').classList.toggle('hidden', p!=='timer');
 document.getElementById('gamecfg').classList.toggle('hidden', p!=='game');
 document.querySelectorAll('button[data-mode]').forEach(b=>
   b.classList.toggle('sel', b.dataset.mode===p));
 gamePolling(p==='game');
}
function choose(p){ setSel(p); }   // Text/Timer: nur Panel oeffnen, noch nichts senden

// Gruen: Button, der zum tatsaechlichen Anzeige-Zustand passt.
function highlight(st){
 const mode=st.mode||'', txt=st.text||'', isText=mode.indexOf('text')>=0;
 document.querySelectorAll('button[data-txt],button[data-mode]').forEach(b=>{
  let on=false;
  if(b.dataset.txt!==undefined)      on = (isText && txt===b.dataset.txt);
  else if(b.dataset.mode==='clock')  on = (mode==='clock');
  else if(b.dataset.mode==='timer')  on = (mode==='timer');
  else if(b.dataset.mode==='game')   on = (mode==='game');
  else if(b.dataset.mode==='idle')   on = (mode==='idle');
  else if(b.dataset.mode==='text')   on = (isText && !PRESETS.includes(txt));
  b.classList.toggle('on',on);
 });
}
// Zusatz-LEDs: aktiven Zustand-Button je Zeile hervorheben + Warnhinweis.
function highlightLeds(st){
 if(st.led1===undefined) return;   // Antwort ohne LED-Felder (z. B. Preset-Route)
 document.querySelectorAll('button[data-led]').forEach(b=>{
  const cur = b.dataset.led==='1'?st.led1:st.led2;
  b.classList.toggle('on', cur===b.dataset.s);
 });
 // Live-Statuspunkte neben dem Titel: effektive Vorschau p1/p2 (inkl. Countdown-
 // Warnung: blink / fast / on), LED1/LED2 in den konfigurierten Farben.
 document.querySelectorAll('.dot').forEach(d=>{
  const p = d.dataset.led==='1'?st.p1:st.p2;
  d.classList.toggle('on', p==='on');
  d.classList.toggle('blink', p==='blink');
  d.classList.toggle('fast', p==='fast');
  // Bei Wechselblinken LED 2 gegenphasig (halbe Periode versetzt, je nach Tempo)
  const blinking = (p==='blink'||p==='fast');
  d.style.animationDelay = (st.alt && d.dataset.led==='2' && blinking)
    ? (p==='fast'?'-.15s':'-.5s') : '';
 });
 const ba=document.getElementById('btnalt'); if(ba) ba.classList.toggle('on', !!st.alt);
 document.getElementById('ledwarn').textContent = st.warn ? tr('warnActive') : '';
}
async function api(u){
 const txt=await (await fetch(u,{cache:'no-store'})).text();
 let st={}; try{st=JSON.parse(txt);}catch(e){}
 highlight(st);
 highlightLeds(st);
 timerButtons(st);
 gameButtons(st);
 return st;
}
function preset(n){ setSel(null); api('/api/preset?name='+n); }
function clock(){ setSel(null); api('/api/cmd?action=clock&value=on'); }
function sendText(){ api('/api/text?msg='+encodeURIComponent(t.value)+'&scroll='+(s.checked?1:0)); }
// Play startet neu (Richtung aus dem Hochzaehlen-Schalter) oder setzt fort,
// wenn der Timer gerade pausiert ist (siehe timerButtons()/timerIsPaused).
let timerIsPaused=false;
function timerPlay(){
 if(timerIsPaused){ api('/api/timer?resume=1'); return; }
 localStorage.setItem('timerMin',min.value);
 localStorage.setItem('timerDir',tdir.checked?'1':'0');
 api('/api/timer?seconds='+(min.value*60)+'&dir='+(tdir.checked?'up':'down'));
}
function timerPause(){ api('/api/timer?pause=1'); }
function stopTimer(){ api('/api/timer?stop=1'); }
// Play/Pause/Stop je nach Zustand (aus/laeuft/pausiert) sperren -- klassisches
// Player-Verhalten statt drei immer aktiver Buttons.
function timerButtons(st){
 const running = st.mode==='timer';
 timerIsPaused = running && !!st.timerPaused;
 tPlay.disabled  = running && !timerIsPaused;
 tPause.disabled = !running || timerIsPaused;
 tStop.disabled  = !running;
 tdir.disabled   = running;
}
(function(){
 const tm=localStorage.getItem('timerMin'); if(tm) min.value=tm;      // zuletzt genutzte Dauer
 if(localStorage.getItem('timerDir')==='1') tdir.checked=true;        // zuletzt genutzte Richtung
})();
function gameCmd(c){ api('/api/game?cmd='+c); }
function gameButtons(st){
 const running = st.mode==='game';
 const bombs = st.gameBombs===undefined?2:st.gameBombs;
 gStats.textContent = 'Score: '+(st.gameScore||0)+' · Leben: '+(st.gameLives===undefined?3:st.gameLives)+' · Bomben: '+bombs+' · Best: '+(st.gameTop||0);
 gUp.disabled = gDown.disabled = gFire.disabled = !running;   // Start/Reset bleiben immer klickbar
 gBomb.disabled = !running || bombs<=0;
}
// Score/Leben brauchen waehrend des Spiels schnellere Updates als der normale
// 3s-State-Poll -- statt eines zweiten, parallelen Timers wird der bestehende
// stateTimer (siehe startPolling()) einfach umgetaktet, solange das Spiel-
// Panel offen ist. So gibt es weiterhin nur einen /api/state-Poller.
function statePollRate(){ return sel==='game' ? 400 : 3000; }
function setStateInterval(ms){
 clearInterval(stateTimer);
 stateTimer=setInterval(()=>api('/api/state'),ms);
}
function gamePolling(on){
 if(stateTimer) setStateInterval(on?400:3000);   // nur umtakten, wenn der Poller ueberhaupt laeuft (Tab sichtbar)
}
// Pfeiltasten/Leertaste steuern das Spiel, aber nur wenn dessen Panel offen ist.
document.addEventListener('keydown',e=>{
 if(sel!=='game') return;
 if(e.key==='ArrowUp')        { e.preventDefault(); gameCmd('up'); }
 else if(e.key==='ArrowDown') { e.preventDefault(); gameCmd('down'); }
 else if(e.key===' ')         { e.preventDefault(); gameCmd('fire'); }
 else if(e.key==='b'||e.key==='B') { e.preventDefault(); gameCmd('bomb'); }
});
function led(i,s){ api('/api/led?i='+i+'&s='+s); }
function ledAlt(){ // umschalten: aktiv -> aus, sonst Wechselblinken ein
 api('/api/led?i=alt&s='+(document.getElementById('btnalt').classList.contains('on')?'off':'on'));
}

// Beim Laden Panel passend zum aktuellen Modus oeffnen.
api('/api/state').then(st=>{
 if(st.mode==='timer') setSel('timer');
 else if(st.mode==='game') setSel('game');
 else if((st.mode||'').indexOf('text')>=0 && !PRESETS.includes(st.text||'')) setSel('text');
});
// ---- Konfigurierbare Darstellungsfarben (LED1/LED2/Matrix) ----
let matrixColor='#ff3b30', matrixOff='rgb(36,20,20)';
function dim(hex,f){ const n=parseInt(hex.slice(1),16);
 return 'rgb('+Math.round(((n>>16)&255)*f)+','+Math.round(((n>>8)&255)*f)+','+Math.round((n&255)*f)+')'; }
function applyColors(c){
 const R=document.documentElement.style;
 if(c.led1){ R.setProperty('--led1',c.led1); R.setProperty('--led1d',dim(c.led1,.22)); }
 if(c.led2){ R.setProperty('--led2',c.led2); R.setProperty('--led2d',dim(c.led2,.22)); }
 if(c.matrix){ matrixColor=c.matrix; matrixOff=dim(c.matrix,.14); }
 if(c.name){ document.getElementById('ttl').textContent=c.name; document.title=c.name; }
}
fetch('/api/appearance',{cache:'no-store'}).then(r=>r.json()).then(applyColors).catch(()=>{});

// ---- Live-Vorschau der LED-Matrix (32x8), adaptiv ----
let framePrev='', stable=0, frameTimer=null;
function drawFrame(cols){
 cols.reverse();   // getColumn(0) ist die rechte Seite -> fuer L->R umdrehen
 const cv=document.getElementById('disp'),x=cv.getContext('2d');
 const W=cv.width,Hh=cv.height,C=cols.length||32,R=8;
 const cw=W/C,ch=Hh/R,r=Math.min(cw,ch)*0.42;
 x.fillStyle='#0a0a0a';x.fillRect(0,0,W,Hh);
 for(let c=0;c<C;c++)for(let row=0;row<R;row++){
  x.beginPath();x.arc(c*cw+cw/2,row*ch+ch/2,r,0,7);
  x.fillStyle=((cols[c]>>row)&1)?matrixColor:matrixOff;x.fill();
 }
}
async function frame(){
 let delay=1000;
 try{
  const txt=await (await fetch('/api/frame',{cache:'no-store'})).text();
  if(txt===framePrev){ if(stable<30)stable++; } else { stable=0; framePrev=txt; }
  drawFrame(JSON.parse(txt));
  delay = stable>=4 ? 1000 : 150;   // 4 gleiche Frames -> Bild ruht -> langsam pollen
 }catch(e){}
 frameTimer = document.hidden ? null : setTimeout(frame,delay);
}

// ---- Akkuanzeige im Titelbereich (nur sichtbar, wenn BATTERY_MONITOR_ENABLED) ----
async function battTick(){
 try{
  const h=await (await fetch('/api/health',{cache:'no-store'})).json();
  const el=document.getElementById('battHd');
  if(!h.battEnabled){ el.style.display='none'; return; }
  el.style.display='';
  const fill=document.getElementById('battFill');
  fill.style.width=Math.max(0,Math.min(100,h.battPct))+'%';
  fill.classList.toggle('low', !!h.battLow);
  document.getElementById('battPct').textContent=h.battPct+' %';
  el.title=h.battVoltage.toFixed(2)+' V';
 }catch(e){}
}

// ---- Poller mit Pause bei unsichtbarem Tab ----
let stateTimer=null, battTimer=null;
function startPolling(){
 if(!stateTimer) stateTimer=setInterval(()=>api('/api/state'),3000);
 api('/api/state');                 // sofort aktualisieren
 if(!frameTimer) frame();
 if(!battTimer) battTimer=setInterval(battTick,10000);   // Akkuspannung aendert sich langsam
 battTick();
}
function stopPolling(){
 clearInterval(stateTimer); stateTimer=null;
 clearTimeout(frameTimer);  frameTimer=null;
 clearInterval(battTimer);  battTimer=null;
}
document.addEventListener('visibilitychange',()=>document.hidden?stopPolling():startPolling());
startPolling();
</script></body></html>)HTML";

// Kombinierte Einstellungsseite mit Registerkarten (LED / WiFi / MQTT).
// Nutzt dieselben API-Routen wie zuvor; IDs/Funktionen sind pro Tab getrennt
// benannt, um Kollisionen zu vermeiden. Tabs laden ihre Daten beim ersten Oeffnen.
static const char SETTINGS_PAGE[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title data-i18n="settingsTitle">Einstellungen</title><link rel="stylesheet" href="/style.css">
<link rel="icon" href="/favicon.ico" sizes="any">
<link rel="icon" type="image/png" sizes="32x32" href="/favicon.png">
<link rel="apple-touch-icon" href="/apple-touch-icon.png">
<link rel="manifest" href="/manifest.json">
<meta name="theme-color" content="#0f172a">
<script>(function(){var t=localStorage.getItem('theme');if(t)document.documentElement.setAttribute('data-theme',t);})();</script>
</head><body>
<h1 data-i18n="settingsTitle">Einstellungen</h1>
<div class="tabs">
 <button class="tab" data-tab="system" onclick="showTab('system')" data-i18n="tabSystem">System</button>
 <button class="tab" data-tab="led" onclick="showTab('led')" data-i18n="tabLed">LED</button>
 <button class="tab" data-tab="wifi" onclick="showTab('wifi')" data-i18n="tabWifi">WiFi</button>
 <button class="tab" data-tab="mqtt" onclick="showTab('mqtt')" data-i18n="tabMqtt">MQTT</button>
</div>

<div id="tab-system" class="panel hidden">
 <p id="sysstatus" data-i18n="loading">Wird geladen ...</p>
 <label class="f"><span data-i18n="dispName">Anzeigename (Titel der Startseite)</span><input type="text" id="dname" maxlength="40" placeholder="Pixel Status" onchange="nameApply()"></label>
 <label class="f"><span data-i18n="hostname">Hostname (erreichbar als &lt;name&gt;.local)</span><input type="text" id="dhost" placeholder="pixelstatus"></label>
 <p class="hv" data-i18n="nameHint">Der Anzeigename wird sofort übernommen. Eine Änderung des Hostnamens erfordert einen Neustart.</p>
 <h2 data-i18n="time">Uhrzeit</h2>
 <p class="hv" id="timenow"></p>
 <div class="switch"><span data-i18n="ntpSync">Zeit per NTP synchronisieren</span>
  <label class="tgl"><input type="checkbox" id="ntpen" onchange="ntpToggle()"><span class="sl"></span></label></div>
 <label class="f" id="ntpsrvwrap"><span data-i18n="ntpServer">NTP-Server</span><input type="text" id="ntpsrv" placeholder="pool.ntp.org"></label>
 <div id="manualwrap" class="hidden">
  <label class="f"><span data-i18n="setManual">Uhrzeit manuell setzen</span><input type="datetime-local" id="mtime"></label>
  <button onclick="setManualTime()" data-i18n="applyTime">Zeit übernehmen</button>
  <p class="hv" data-i18n="noNtpHint">Ohne NTP (und ohne RTC) geht die Zeit bei jedem Neustart verloren.</p>
 </div>
 <button onclick="sysSave()" data-i18n="save">Speichern</button>
 <p id="sysmsg"></p>
 <h2 data-i18n="bootAnimTitle">Einschalt-Animation</h2>
 <label class="f"><span data-i18n="bootAnimLabel">Animation beim Einschalten</span>
  <select id="bootanim" onchange="bootAnim()">
   <option value="0" data-i18n="bootOff">Aus</option>
   <option value="1" data-i18n="bootScan">Scan-Wipe</option>
   <option value="2" data-i18n="bootFill">Pixel-Fill</option>
   <option value="3" data-i18n="bootProgress">Fortschrittsbalken (WiFi-Verbindung)</option>
  </select>
 </label>
 <button onclick="bootAnimPreview()" data-i18n="bootPreviewBtn">Vorschau</button>
 <div id="battwrap" class="hidden">
  <h2 data-i18n="battery">Akku</h2>
  <p class="hv" id="battnow" data-i18n="battNow">Aktuell angezeigt: -</p>
  <p class="hv" data-i18n="battHint">Mit einem Multimeter direkt an <code>OUT+</code>/<code>OUT-</code> (bzw. den
   Akku-Pluspol/Minuspol) messen und den Wert hier eintragen. Gleicht Bauteiltoleranzen im
   A0-Spannungsteiler aus, wirkt sofort, kein Neustart nötig.</p>
  <label class="f"><span data-i18n="realVolt">Echte Spannung (Multimeter)</span><input type="number" id="battvolts" step="0.01" min="2.5" max="4.3" data-i18n-ph="battVoltPh" placeholder="z.B. 4.08"></label>
  <button onclick="battCalibrate()" data-i18n="calibrate">Kalibrieren</button>
  <p id="battmsg"></p>
 </div>
</div>

<div id="tab-led" class="panel hidden">
 <div class="ledlegend">
  <span><span class="swatch s1"></span>LED 1</span>
  <span><span class="swatch s2"></span>LED 2</span>
 </div>
 <p id="ledstatus" data-i18n="loading">Wird geladen ...</p>
 <label class="f"><span data-i18n="blinkSpeed">Blinkgeschwindigkeit (manuell, ms je Zyklus)</span><input type="number" id="blinkp" min="100" max="5000" step="50" value="500" onchange="ledSave()"></label>
 <h2><span data-i18n="ledBright">Helligkeit LEDs</span> <span id="lbv" class="hv"></span></h2>
 <input type="range" id="lb" min="0" max="15" value="15" oninput="lbval()" onchange="ledBright()">
 <h2><span data-i18n="matrixBright">Helligkeit Matrix</span> <span id="bv" class="hv"></span></h2>
 <input type="range" id="b" min="0" max="15" value="4" oninput="bval()" onchange="bright()">
 <label class="f"><span data-i18n="orientLabel">Ausrichtung der Schrift</span>
  <select id="orient" onchange="orient()">
   <option value="0" data-i18n="orient0">Normal (0°)</option>
   <option value="180" data-i18n="orient180">180° (auf dem Kopf)</option>
  </select>
 </label>
 <label class="f"><span data-i18n="scrollDirLabel">Scrollrichtung</span>
  <select id="scrolldir" onchange="scrollDir()">
   <option value="left" data-i18n="scrollLeft">Links (Standard)</option>
   <option value="right" data-i18n="scrollRight">Rechts</option>
  </select>
 </label>
 <div class="switch"><span data-i18n="autoWarn">Countdown-Warnung (blinken)</span>
  <label class="tgl"><input type="checkbox" id="aw" onchange="ledSave()"><span class="sl"></span></label></div>
 <label class="f"><span data-i18n="warnMin">Warnfenster in Minuten (ab hier blinken beide LEDs)</span><input type="number" id="warn" min="1" value="5" onchange="ledSave()"></label>
 <label class="f"><span data-i18n="fastSecs">Schnell-Blink in den letzten ... Sekunden (0 = aus)</span><input type="number" id="fast" min="0" value="60" onchange="ledSave()"></label>
 <div class="switch"><span data-i18n="altBlink">Wechselblinken (LEDs abwechselnd)</span>
  <label class="tgl"><input type="checkbox" id="alt" onchange="ledSave()"><span class="sl"></span></label></div>
 <div class="switch"><span data-i18n="activeLow">LEDs invertiert (Active-Low)</span>
  <label class="tgl"><input type="checkbox" id="al" onchange="ledSave()"><span class="sl"></span></label></div>
 <h2 data-i18n="webColors">Farben (Web-Darstellung)</h2>
 <p class="hv" data-i18n="colorsHint">Passe die Farben an die tatsächlich verbauten LEDs bzw. das Matrix-Panel an.</p>
 <label class="f"><span data-i18n="colorLed1">Farbe LED 1</span><input type="color" id="c1" value="#ef4444" oninput="preview()" onchange="ledSave()"></label>
 <label class="f"><span data-i18n="colorLed2">Farbe LED 2</span><input type="color" id="c2" value="#22c55e" oninput="preview()" onchange="ledSave()"></label>
 <label class="f"><span data-i18n="colorMatrix">Farbe Matrix-Vorschau</span><input type="color" id="cm" value="#ff3b30" oninput="preview()" onchange="ledSave()"></label>
 <p id="ledmsg"></p>
</div>

<div id="tab-wifi" class="panel hidden">
 <p id="wifistatus" data-i18n="loading">Wird geladen ...</p>
 <h2 data-i18n="selectNet">Netz wählen</h2>
 <select id="sel" onchange="ssid.value=this.value"></select>
 <button onclick="wifiScan()" data-i18n="rescan">Erneut scannen</button>
 <h2 data-i18n="connect">Verbinden</h2>
 <label class="f">SSID<input type="text" id="ssid" placeholder="SSID"></label>
 <label class="f"><span data-i18n="wifiPass">Passwort (leer = offenes Netz)</span><input type="password" id="wifipw"></label>
 <button onclick="wifiSave()" data-i18n="saveRestart">Speichern &amp; Neustart</button>
 <p id="wifimsg"></p>
</div>

<div id="tab-mqtt" class="panel hidden">
 <p id="mqttstatus" data-i18n="loading">Wird geladen ...</p>
 <div class="switch"><span data-i18n="mqttEnabled">MQTT aktiviert</span>
  <label class="tgl"><input type="checkbox" id="en"><span class="sl"></span></label></div>
 <label class="f"><span data-i18n="mqttHost">Broker-Host / IP</span><input type="text" id="mqtthost" data-i18n-ph="mqttHostPh" placeholder="z.B. 192.168.1.10"></label>
 <label class="f">Port<input type="number" id="port" value="1883"></label>
 <label class="f"><span data-i18n="mqttUser">Benutzer (optional)</span><input type="text" id="user"></label>
 <label class="f"><span data-i18n="mqttPass">Passwort (leer = unveraendert)</span><input type="password" id="mqttpw"></label>
 <label class="f"><span data-i18n="mqttBase">Basis-Topic</span><input type="text" id="base" placeholder="pixelstatus"></label>
 <button onclick="mqttSave()" data-i18n="saveRestart">Speichern &amp; Neustart</button>
 <p id="mqttmsg"></p>
</div>

<button class="wide" onclick="location.href='/'" data-i18n="back">&larr; Zurück zur Steuerung</button>
<script src="/lang.js"></script>
<script>
// ---- Registerkarten ----
const NOCACHE={cache:'no-store'};   // Status-Fetches nie aus dem Browser-Cache
let inited={system:false,led:false,wifi:false,mqtt:false};
function showTab(t){
 ['system','led','wifi','mqtt'].forEach(x=>{
  document.getElementById('tab-'+x).classList.toggle('hidden', x!==t);
  document.querySelector('.tab[data-tab="'+x+'"]').classList.toggle('active', x===t);
 });
 if(!inited[t]){ inited[t]=true;
  if(t==='system'){sysLoad();battLoad();} else if(t==='led')ledLoad(); else if(t==='wifi')wifiInit(); else mqttInit(); }
 try{ history.replaceState(null,'','#'+t); }catch(e){}
}
function initialTab(){
 const h=(location.hash||'').replace('#','');
 if(h==='system'||h==='led'||h==='wifi'||h==='mqtt') return h;
 const p=location.pathname;
 if(p.indexOf('wifi')>=0) return 'wifi';
 if(p.indexOf('mqtt')>=0) return 'mqtt';
 if(p.indexOf('leds')>=0) return 'led';
 return 'system';
}

// ---- System-Tab ----
// Lokale Browserzeit im datetime-local-Format (YYYY-MM-DDTHH:mm) -- der
// manuelle Zeit-Regler wird daraus vorbefuellt statt aus der (evtl. falschen
// oder abgelaufenen) Geraetezeit, dann reicht meist ein Klick auf "Uebernehmen".
function localNowStr(){
 const d=new Date();
 return new Date(d.getTime()-d.getTimezoneOffset()*60000).toISOString().slice(0,16);
}
// Blendet NTP-Server bzw. manuelle Zeit passend zum Schalter ein/aus.
function ntpToggle(){
 const on=document.getElementById('ntpen').checked;
 document.getElementById('ntpsrvwrap').classList.toggle('hidden', !on);
 document.getElementById('manualwrap').classList.toggle('hidden', on);
 if(!on) document.getElementById('mtime').value=localNowStr();   // frisch bei jedem Umschalten auf manuell
}
async function sysLoad(){
 try{
  const c=await (await fetch('/system/status',NOCACHE)).json();
  dname.value=c.name; dhost.value=c.hostname;
  document.getElementById('ntpen').checked=c.ntpEnabled;
  document.getElementById('ntpsrv').value=c.ntpServer;
  document.getElementById('mtime').value=localNowStr();   // Browserzeit vorbefuellen
  document.getElementById('timenow').textContent=tr('deviceTime')+c.time.replace('T',' ')
    +(c.synced?' ('+tr('synced')+')':' ('+tr('notSynced')+')');
  ntpToggle();
  if(c.bootAnim!=null) document.getElementById('bootanim').value=c.bootAnim;
  document.getElementById('sysstatus').textContent=tr('hostnamePrefix')+c.hostname+'.local';
 }catch(e){ document.getElementById('sysstatus').textContent=tr('statusUnavail'); }
}
// Auswahl aendern UND "Vorschau" loesen dieselbe Aktion aus: setBootAnimation()
// (Firmware) persistiert nur bei Aenderung, spielt aber immer sofort einmal ab.
function bootAnim(){ fetch('/api/bootanim?type='+document.getElementById('bootanim').value); }
function bootAnimPreview(){ bootAnim(); }
// Anzeigename sofort uebernehmen (wie Helligkeit/Ausrichtung) -- Hostname/NTP
// bleiben ueber sysSave() batched, da ein Hostnamenwechsel einen Neustart ausloest.
async function nameApply(){
 try{ await fetch('/system/save?name='+encodeURIComponent(dname.value));
  document.getElementById('sysmsg').textContent=tr('savedApplied'); }
 catch(e){ document.getElementById('sysmsg').textContent=tr('saveFailed'); }
}
async function sysSave(){
 const en=document.getElementById('ntpen').checked;
 const q='/system/save?name='+encodeURIComponent(dname.value)+'&host='+encodeURIComponent(dhost.value)
   +'&ntpEnabled='+(en?1:0)+'&ntpServer='+encodeURIComponent(document.getElementById('ntpsrv').value);
 try{
  const r=await (await fetch(q)).json();
  document.getElementById('sysmsg').textContent = r.restart
    ? tr('savedRestartHost')
    : tr('saved');
  if(!r.restart) setTimeout(sysLoad,300);   // Zeitanzeige aktualisieren
 }catch(e){ document.getElementById('sysmsg').textContent=tr('saveFailed'); }
}
// Manuelle Zeit sofort setzen und NTP dabei deaktivieren (sonst ueberschreibt es
// der naechste Sync). datetime-local ist lokale Zeit -> in Epoch-Sekunden wandeln.
async function setManualTime(){
 const v=document.getElementById('mtime').value;
 if(!v){ document.getElementById('sysmsg').textContent=tr('pickTime'); return; }
 const epoch=Math.floor(new Date(v).getTime()/1000);
 try{
  await fetch('/system/save?ntpEnabled=0&settime='+epoch);
  document.getElementById('ntpen').checked=false; ntpToggle();
  document.getElementById('sysmsg').textContent=tr('timeSetNoNtp');
  setTimeout(sysLoad,300);
 }catch(e){ document.getElementById('sysmsg').textContent=tr('setFailed'); }
}

// ---- Akku-Kalibrierung (Teil des System-Tabs) ----
async function battLoad(){
 try{
  const b=await (await fetch('/battery/status',NOCACHE)).json();
  if(!b.enabled) return;   // BATTERY_MONITOR_ENABLED aus -> Block bleibt versteckt
  document.getElementById('battwrap').classList.remove('hidden');
  document.getElementById('battnow').textContent=tr('currentlyShown')+b.voltage.toFixed(2)+' V ('+b.percent+' %)';
 }catch(e){}
}
async function battCalibrate(){
 const v=document.getElementById('battvolts').value;
 const msg=document.getElementById('battmsg');
 if(!v){ msg.textContent=tr('pickVolts'); return; }
 try{
  const r=await (await fetch('/battery/calibrate?volts='+encodeURIComponent(v))).json();
  msg.textContent = r.saved
    ? tr('calibratedTo')+parseFloat(v).toFixed(2)+' V.'
    : tr('calibFailedNoMeasure');
  if(r.saved) battLoad();
 }catch(e){ msg.textContent=tr('calibFailed'); }
}

// ---- LED-Tab ----
function dim(hex,f){ const n=parseInt(hex.slice(1),16);
 return 'rgb('+Math.round(((n>>16)&255)*f)+','+Math.round(((n>>8)&255)*f)+','+Math.round((n&255)*f)+')'; }
function applyColors(){
 const R=document.documentElement.style;
 R.setProperty('--led1',c1.value); R.setProperty('--led1d',dim(c1.value,.22));
 R.setProperty('--led2',c2.value); R.setProperty('--led2d',dim(c2.value,.22));
}
function preview(){ applyColors(); }
function bval(){ document.getElementById('bv').textContent=b.value; }
function lbval(){ document.getElementById('lbv').textContent=lb.value; }
function bright(){ fetch('/api/brightness?level='+b.value); }
function ledBright(){ fetch('/api/ledbrightness?level='+lb.value); }
function orient(){ fetch('/api/orientation?deg='+document.getElementById('orient').value); }
function scrollDir(){ fetch('/api/scrolldir?dir='+document.getElementById('scrolldir').value); }
async function ledLoad(){
 const g=id=>document.getElementById(id);
 try{
  const c=await (await fetch('/leds/status',NOCACHE)).json();
  g('warn').value=Math.round(c.warnSecs/60);   // zuerst befuellen (unabhaengig vom Rest)
  g('fast').value=c.warnFastSecs;
  g('blinkp').value=c.blinkPeriod;
  g('lb').value=c.ledBrightness; lbval();
  g('aw').checked=c.autoWarn; g('al').checked=c.activeLow; g('alt').checked=c.alternate;
  g('ledstatus').textContent=c.warn?tr('warningActive'):'';
 }catch(e){ g('ledstatus').textContent=tr('statusUnavail'); }
 try{ const s=await (await fetch('/api/state',NOCACHE)).json();
  if(s.brightness!=null){ g('b').value=s.brightness; bval(); }
  if(s.orientation!=null) g('orient').value=s.orientation;
  if(s.scrollReverse!=null) g('scrolldir').value=s.scrollReverse?'right':'left'; }catch(e){}
 try{
  const a=await (await fetch('/api/appearance',NOCACHE)).json();
  g('c1').value=a.led1; g('c2').value=a.led2; g('cm').value=a.matrix; applyColors();
 }catch(e){}
}
async function ledSave(){
 const q='/leds/save?autoWarn='+(aw.checked?1:0)+'&activeLow='+(al.checked?1:0)
  +'&alternate='+(alt.checked?1:0)
  +'&warnSecs='+(Math.max(1,warn.value*60))+'&warnFastSecs='+Math.max(0,fast.value)
  +'&blinkPeriod='+Math.min(5000,Math.max(100,blinkp.value))
  +'&ledBrightness='+Math.min(15,Math.max(0,lb.value))
  +'&led1='+encodeURIComponent(c1.value)+'&led2='+encodeURIComponent(c2.value)
  +'&matrix='+encodeURIComponent(cm.value);
 try{ await fetch(q); document.getElementById('ledmsg').textContent=tr('savedApplied'); }
 catch(e){ document.getElementById('ledmsg').textContent=tr('saveFailed'); }
}

// ---- WiFi-Tab ----
async function wifiLoadStatus(){
 const el=document.getElementById('wifistatus');
 try{
  const c=await (await fetch('/wifi/status',NOCACHE)).json();
  el.textContent = c.connected
    ? (tr('connectedTo')+'"'+c.ssid+'"  ('+c.ip+')')
    : (c.ap ? tr('apActive') : tr('notConnected'));
 }catch(e){ el.textContent=tr('statusUnavail'); }
}
// Scan anstossen + Ergebnis pollen (FUNC-03: /wifi/scan blockiert seit dem Fix
// nicht mehr, sondern startet nur und liefert sofort zurueck). Bis zu ~10s in
// 500ms-Schritten pollen, dann als fehlgeschlagen behandeln.
async function wifiScan(){
 const s=document.getElementById('sel');s.innerHTML='<option>'+tr('scanning')+'</option>';
 try{
  await fetch('/wifi/scan',NOCACHE);
  for(let tries=0; tries<20; tries++){
   await new Promise(r=>setTimeout(r,500));
   const r=await (await fetch('/wifi/scan/result',NOCACHE)).json();
   if(r.status==='done'){
    const nets=r.networks;
    nets.sort((a,b)=>b.rssi-a.rssi);s.innerHTML='';
    for(const n of nets){const o=document.createElement('option');o.value=n.ssid;
     o.textContent=n.ssid+'  ('+n.rssi+' dBm)';s.appendChild(o);}
    if(nets.length)ssid.value=nets[0].ssid;
    return;
   }
   if(r.status==='failed'){ s.innerHTML='<option>'+tr('scanFailed')+'</option>'; return; }
  }
  s.innerHTML='<option>'+tr('scanFailed')+'</option>';
 }catch(e){s.innerHTML='<option>'+tr('scanFailed')+'</option>';}
}
async function wifiSave(){
 if(!ssid.value){document.getElementById('wifimsg').textContent=tr('pickSsid');return;}
 document.getElementById('wifimsg').textContent=tr('savedRestartConnect');
 // POST statt GET: WLAN-Passwort landet sonst im Klartext in der Request-URL
 // (Browser-Verlauf, ggf. Proxy-/Router-Logs). _server.arg() liest Formular-
 // Body und Query-String gleich, serverseitig ist also keine Aenderung noetig.
 fetch('/wifi/save',{method:'POST',body:new URLSearchParams({ssid:ssid.value,pass:wifipw.value})});
}
function wifiInit(){ wifiLoadStatus(); wifiScan(); }

// ---- MQTT-Tab ----
function mqttShow(c){
 document.getElementById('mqttstatus').textContent = !c.enabled ? tr('mqttDisabled')
   : (c.connected ? (tr('connectedTo')+c.host+':'+c.port)
                  : tr('mqttEnabledNotConn'));
}
async function mqttLoad(){
 try{
  const c=await (await fetch('/mqtt/status',NOCACHE)).json();
  en.checked=c.enabled;mqtthost.value=c.host;port.value=c.port;
  user.value=c.user;base.value=c.base;
  mqttShow(c);
 }catch(e){}
}
async function mqttRefresh(){
 try{ mqttShow(await (await fetch('/mqtt/status',NOCACHE)).json()); }
 catch(e){ document.getElementById('mqttstatus').textContent=tr('statusUnavail'); }
}
async function mqttSave(){
 // POST statt GET, siehe wifiSave() -- MQTT-Passwort nicht in der URL.
 const body=new URLSearchParams({enabled:en.checked?1:0,host:mqtthost.value,port:port.value,
  user:user.value,pass:mqttpw.value,base:base.value});
 document.getElementById('mqttmsg').textContent=tr('savedRestart');
 fetch('/mqtt/save',{method:'POST',body});
}
let mTimer=null;
function mqttInit(){
 mqttLoad();
 if(!mTimer) mTimer=setInterval(mqttRefresh,3000);   // Verbindungsstatus live spiegeln
}
document.addEventListener('visibilitychange',()=>{
 if(document.hidden){ if(mTimer){clearInterval(mTimer);mTimer=null;} }
 else if(inited.mqtt && !mTimer){ mqttRefresh(); mTimer=setInterval(mqttRefresh,3000); }
});

showTab(initialTab());
</script></body></html>)HTML";


// System-/Diagnose-Seite: Kennzahl-Kacheln + Live-Sparklines (pollt /api/health).
static const char HEALTH_PAGE[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title data-i18n="healthTitle">System &amp; Diagnose</title><link rel="stylesheet" href="/style.css">
<link rel="icon" href="/favicon.ico" sizes="any">
<link rel="icon" type="image/png" sizes="32x32" href="/favicon.png">
<link rel="apple-touch-icon" href="/apple-touch-icon.png">
<link rel="manifest" href="/manifest.json">
<meta name="theme-color" content="#0f172a">
<script>(function(){var t=localStorage.getItem('theme');if(t)document.documentElement.setAttribute('data-theme',t);})();</script>
<style>
.cards{display:grid;grid-template-columns:repeat(2,1fr);gap:.6rem;margin:.6rem 0}
.card{background:var(--card);border-radius:.6rem;padding:.7rem}
.card .k{font-size:.75rem;color:var(--muted)}
.card .v{font-size:1.4rem;margin-top:.2rem}
.g{margin-top:1.1rem}
.gh{display:flex;justify-content:space-between;align-items:baseline}
.gh h3{font-size:.85rem;margin:.3rem 0;color:var(--muted);font-weight:600}
.cur{font-size:1rem}
canvas{width:100%;height:72px;background:var(--card);border-radius:.5rem;display:block}
.info{width:100%;border-collapse:collapse;font-size:.85rem}
.info td{padding:.35rem .2rem;border-top:1px solid var(--border);vertical-align:top}
.info td:first-child{color:var(--muted);width:42%}
.info td:last-child{font-family:monospace;word-break:break-all}
</style></head><body>
<h1 data-i18n="healthTitle">System &amp; Diagnose</h1>
<div class="cards">
 <div class="card"><div class="k" data-i18n="heapFree">Heap frei</div><div class="v" id="v_heap">-</div></div>
 <div class="card"><div class="k" data-i18n="cpuLoad">CPU-Last (geschätzt)</div><div class="v" id="v_cpu">-</div></div>
 <div class="card"><div class="k" data-i18n="frag">Fragmentierung</div><div class="v" id="v_frag">-</div></div>
 <div class="card"><div class="k" data-i18n="wifiSignal">WiFi-Signal</div><div class="v" id="v_rssi">-</div></div>
 <div class="card"><div class="k" data-i18n="uptime">Uptime</div><div class="v" id="v_up">-</div></div>
 <div class="card"><div class="k" data-i18n="loopRate">Loop-Rate</div><div class="v" id="v_lps">-</div></div>
 <div class="card"><div class="k" data-i18n="sketch">Programm</div><div class="v" id="v_sketch">-</div></div>
 <div class="card"><div class="k" data-i18n="fsCard">Dateisystem</div><div class="v" id="v_fs">-</div></div>
 <div class="card" id="card_batt" style="display:none"><div class="k" data-i18n="battCard">Akku</div><div class="v" id="v_batt">-</div></div>
</div>
<div class="g"><div class="gh"><h3 data-i18n="freeHeapH">Freier Heap</h3><span class="cur" id="cur_heap"></span></div><canvas id="c_heap"></canvas></div>
<div class="g"><div class="gh"><h3 data-i18n="cpuLoadH">CPU-Last</h3><span class="cur" id="cur_cpu"></span></div><canvas id="c_cpu"></canvas></div>
<div class="g"><div class="gh"><h3 data-i18n="wifiSignalH">WiFi-Signal</h3><span class="cur" id="cur_rssi"></span></div><canvas id="c_rssi"></canvas></div>
<div class="g" id="g_batt" style="display:none"><div class="gh"><h3 data-i18n="battVoltH">Akkuspannung</h3><span class="cur" id="cur_batt"></span></div><canvas id="c_batt"></canvas></div>
<div class="g"><h3 data-i18n="timeH">Zeit</h3><table class="info"><tbody>
 <tr><td data-i18n="time">Uhrzeit</td><td id="i_time">-</td></tr>
 <tr><td>NTP</td><td id="i_ntp">-</td></tr>
</tbody></table></div>
<div class="g"><h3 data-i18n="network">Netzwerk</h3><table class="info"><tbody id="net"></tbody></table></div>
<div class="g"><h3 data-i18n="fwChip">Firmware &amp; Chip</h3><table class="info"><tbody id="fw"></tbody></table></div>
<p id="msg"></p>
<button class="wide" onclick="location.href='/'" data-i18n="back">&larr; Zurück zur Steuerung</button>
<script src="/lang.js"></script>
<script>
const MAX=60, IDS=['c_heap','c_cpu','c_rssi'], H=[], C=[], R=[], B=[];
let battShown=false;
const el=id=>document.getElementById(id);
function fmtUp(s){const d=s/86400|0,h=s%86400/3600|0,m=s%3600/60|0;return (d?d+'d ':'')+(h?h+'h ':'')+m+'m';}
function push(a,v){a.push(v); if(a.length>MAX)a.shift();}
function sizeAll(){for(const id of IDS){const c=el(id);c.width=c.clientWidth;c.height=72;} if(battShown){const c=el('c_batt');c.width=c.clientWidth;c.height=72;}}
function spark(cv,data,o){
 const x=cv.getContext('2d'),w=cv.width,h=cv.height,top=12,bot=h-16,ph=bot-top;
 x.clearRect(0,0,w,h);
 if(data.length<2)return;
 const mn=o.min!=null?o.min:Math.min(...data), mx=o.max!=null?o.max:Math.max(...data), rg=(mx-mn)||1;
 const X=i=>i/(data.length-1)*(w-6)+3, Y=v=>bot-((v-mn)/rg)*ph;
 x.lineWidth=2;x.strokeStyle=o.color;x.beginPath();
 data.forEach((v,i)=>{const xx=X(i),yy=Y(v);i?x.lineTo(xx,yy):x.moveTo(xx,yy);});
 x.stroke();
 x.lineTo(X(data.length-1),bot);x.lineTo(X(0),bot);x.closePath();
 x.globalAlpha=.15;x.fillStyle=o.color;x.fill();x.globalAlpha=1;
 x.fillStyle=o.muted;x.font='10px system-ui,sans-serif';x.textAlign='left';
 x.fillText(Math.round(mx)+o.unit,4,10);
 x.fillText(Math.round(mn)+o.unit,4,h-4);
}
function draw(){
 const cs=getComputedStyle(document.documentElement);
 const acc=cs.getPropertyValue('--btn').trim()||'#3b82f6', mut=cs.getPropertyValue('--muted').trim()||'#888';
 spark(el('c_heap'),H,{color:acc,muted:mut,unit:' KB'});
 spark(el('c_cpu'),C,{color:'#ef4444',muted:mut,unit:' %',min:0,max:100});
 spark(el('c_rssi'),R,{color:'#f59e0b',muted:mut,unit:' dBm',min:-95,max:-30});
 if(battShown) spark(el('c_batt'),B,{color:'#22c55e',muted:mut,unit:'V'});
}
async function tick(){
 try{
  const h=await (await fetch('/api/health',{cache:'no-store'})).json();
  const kb=b=>Math.round(b/1024);
  const vh=el('v_heap'); vh.textContent=kb(h.heap)+' KB'; vh.style.color=h.heap<15360?'#ef4444':'';
  const vc=el('v_cpu');  vc.textContent=h.cpu+' %';       vc.style.color=h.cpu>85?'#ef4444':'';
  el('v_frag').textContent=h.frag+' %';
  el('v_rssi').textContent=h.rssi+' dBm';
  el('v_up').textContent=fmtUp(h.uptime);
  el('v_lps').textContent=h.lps.toLocaleString(curLang()==='en'?'en-US':'de-DE')+'/s';
  el('v_sketch').textContent=kb(h.sketch)+' KB';
  el('v_fs').textContent=kb(h.fsused)+' / '+kb(h.fstotal)+' KB';
  el('i_time').textContent=h.time;
  el('i_ntp').textContent=h.ntp?tr('synced'):tr('notSynced');
  push(H,kb(h.heap)); push(C,h.cpu); push(R,h.rssi);
  el('cur_heap').textContent=kb(h.heap)+' KB';
  el('cur_cpu').textContent=h.cpu+' %';
  el('cur_rssi').textContent=h.rssi+' dBm';
  if(h.battEnabled){
   if(!battShown){
    battShown=true;
    el('card_batt').style.display='';
    el('g_batt').style.display='';
    const c=el('c_batt'); c.width=c.clientWidth; c.height=72;
   }
   const vb=el('v_batt'); vb.textContent=h.battPct+' % ('+h.battVoltage.toFixed(2)+' V)';
   vb.style.color=h.battLow?'#ef4444':'';
   push(B,h.battVoltage);
   el('cur_batt').textContent=h.battVoltage.toFixed(2)+' V';
  }
  draw();
  el('msg').textContent='';
 }catch(e){ el('msg').textContent=tr('noConn'); }
}
// Kein innerHTML: SSID/Hostname stammen vom Geraet (WiFi.SSID()/Hostname sind
// per Angreifer-AP bzw. /system/save frei setzbar) und wuerden sonst als HTML
// interpretiert (Stored-XSS). textContent rendert sie immer als reinen Text.
function fill(id,rows){
 const t=el(id); t.innerHTML='';
 rows.forEach(r=>{
  const tr=document.createElement('tr');
  const a=document.createElement('td'), b=document.createElement('td');
  a.textContent=r[0]; b.textContent=r[1];
  tr.append(a,b); t.appendChild(tr);
 });
}
async function loadInfo(){
 try{
  const s=await (await fetch('/api/sysinfo',{cache:'no-store'})).json();
  const kb=b=>Math.round(b/1024);
  if(s.apMode){
   fill('net',[['IP',s.ip],[tr('mac'),s.mac],[tr('apSsidRow'),s.ssid],[tr('apClients'),s.clients]]);
  } else {
   fill('net',[['IP',s.ip],[tr('gateway'),s.gw],[tr('subnet'),s.mask],[tr('dns'),s.dns],[tr('mac'),s.mac],
     [tr('hostnameRow'),s.host+'.local'],[tr('ssidRow'),s.ssid],[tr('bssid'),s.bssid],[tr('wifiChannel'),s.ch]]);
  }
  const flash=kb(s.flashreal)+' KB'+(s.flashreal!=s.flashconf?tr('configured')+kb(s.flashconf)+' KB!)':'');
  fill('fw',[[tr('sdk'),s.sdk],[tr('core'),s.core],[tr('cpuClock'),s.cpu+' MHz'],[tr('chipId'),s.chip],
    [tr('build'),s.build],[tr('flash'),flash]]);
 }catch(e){}
}
addEventListener('resize',()=>{sizeAll();draw();});
sizeAll(); loadInfo(); tick();
let hTimer=setInterval(tick,2000);
document.addEventListener('visibilitychange',()=>{
 if(document.hidden){ clearInterval(hTimer); hTimer=null; }
 else if(!hTimer){ tick(); hTimer=setInterval(tick,2000); }
});
</script></body></html>)HTML";

// Touch-optimierte Steuerseite fuer "Pixel Attack" (separater Tab, siehe
// "Touch-Steuerung"-Button im Spiel-Panel der Startseite) -- fuer Handys quer
// gehalten gedacht: grosse Daumen-Buttons links (Hoch/Runter) und rechts
// (Feuer/Bombe), Mitte zeigt Score/Leben/Bomben/Best + Neustart-Button.
// Eigenstaendige Seite statt eines Panels auf der Startseite, weil die Buttons
// dafuer den ganzen Bildschirm ausfuellen sollen (keine Kopfzeile, kein
// Scrollen) -- mit den normalgrossen Buttons der Startseite waere das nicht
// vereinbar. Nutzt dieselbe /api/game-Route wie das Desktop-Panel, keine
// neue Backend-Logik noetig. In Hochformat blendet eine @media-Query einen
// Rotationshinweis ein und versteckt die Spalten, statt mit der Steuerung zu
// verrutschen.
static const char PLAY_PAGE[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="de"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title data-i18n="game">Pixel Attack</title><link rel="stylesheet" href="/style.css">
<link rel="icon" href="/favicon.ico" sizes="any">
<link rel="icon" type="image/png" sizes="32x32" href="/favicon.png">
<link rel="apple-touch-icon" href="/apple-touch-icon.png">
<link rel="manifest" href="/manifest.json">
<meta name="theme-color" content="#0f172a">
<script>(function(){var t=localStorage.getItem('theme');if(t)document.documentElement.setAttribute('data-theme',t);})();</script>
<style>
*{box-sizing:border-box}
html,body{height:100%;margin:0;overflow:hidden;overscroll-behavior:none}
body{display:flex;max-width:none;width:100%;background:var(--bg);color:var(--fg);font-family:system-ui,sans-serif;
 touch-action:manipulation;-webkit-user-select:none;user-select:none;-webkit-touch-callout:none;
 padding:env(safe-area-inset-top) env(safe-area-inset-right) env(safe-area-inset-bottom) env(safe-area-inset-left)}
.col{flex:1;display:flex;flex-direction:column;gap:.6rem;padding:.5rem;min-width:0}
.side button{flex:1;font-size:min(14vw,4rem);border:0;border-radius:1rem;background:var(--btn);color:#fff;padding:0}
.side button:active{background:var(--btna)}
.side button:disabled{opacity:.3}
#pBomb{background:var(--grey)}
.mid{flex:0 0 30%;align-items:center;justify-content:center;text-align:center}
.stat{width:100%}
.stat .k{font-size:.7rem;color:var(--muted);text-transform:uppercase;letter-spacing:.04em}
.stat .v{font-size:1.6rem;font-variant-numeric:tabular-nums}
#pReset{margin-top:.5rem;font-size:1.3rem;padding:.7rem 1rem;border:0;border-radius:.8rem;background:var(--grey);color:#fff;width:100%}
.hint{display:none;position:fixed;inset:0;flex-direction:column;align-items:center;justify-content:center;gap:1.4rem;text-align:center;
 padding:2rem;font-size:1.3rem;background:var(--bg);color:var(--fg);z-index:1}
.hint button{font-size:1rem;padding:.7rem 1.4rem;border:0;border-radius:.7rem;background:var(--grey);color:#fff}
@media (orientation:portrait){ .hint{display:flex} .col{visibility:hidden} }
</style></head><body>
<div class="hint">
 <div data-i18n="rotateHint">Bitte Handy quer halten</div>
 <button onclick="window.close()" data-i18n="closeTab">Tab schließen</button>
</div>
<div class="col side">
 <button id="pUp" onclick="gameCmd('up')">&#9650;</button>
 <button id="pDown" onclick="gameCmd('down')">&#9660;</button>
</div>
<div class="col mid">
 <div class="stat"><div class="k" data-i18n="statScore">Score</div><div class="v" id="pScore">0</div></div>
 <div class="stat"><div class="k" data-i18n="statLives">Leben</div><div class="v" id="pLives">3</div></div>
 <div class="stat"><div class="k" data-i18n="statBombs">Bomben</div><div class="v" id="pBombs">2</div></div>
 <div class="stat"><div class="k" data-i18n="statTop">Best</div><div class="v" id="pTop">0</div></div>
 <button id="pReset" onclick="gameCmd('start')" data-i18n="reset">Neustart</button>
</div>
<div class="col side">
 <button id="pFire" onclick="gameCmd('fire')">&#9679;</button>
 <button id="pBomb" onclick="gameCmd('bomb')">&#10033;</button>
</div>
<script src="/lang.js"></script>
<script>
function gameCmd(c){ fetch('/api/game?cmd='+c,{cache:'no-store'}).catch(()=>{}); }
async function poll(){
 try{
  const st=await (await fetch('/api/state',{cache:'no-store'})).json();
  const running = st.mode==='game';
  pScore.textContent = st.gameScore||0;
  pLives.textContent = st.gameLives===undefined?3:st.gameLives;
  pBombs.textContent = st.gameBombs===undefined?2:st.gameBombs;
  pTop.textContent   = st.gameTop||0;
  pUp.disabled = pDown.disabled = pFire.disabled = !running;
  pBomb.disabled = !running || (st.gameBombs||0)<=0;
 }catch(e){}
}
let pTimer=null;
function startPolling(){ if(!pTimer){ poll(); pTimer=setInterval(poll,400); } }
function stopPolling(){ clearInterval(pTimer); pTimer=null; }
document.addEventListener('visibilitychange',()=>document.hidden?stopPolling():startPolling());
startPolling();
</script></body></html>)HTML";

// PWA-Manifest (siehe /icon-192.png, /icon-512.png in Icons.h). Name bewusst
// fest "Pixel Status" -- der zur Laufzeit einstellbare Anzeigename (siehe
// /api/appearance) ist rein kosmetisch fuer die Startseite und muesste sonst
// bei jedem Aufruf neu zusammengebaut werden, fuer ein App-Icon-Label lohnt
// sich das nicht. Ob Browser daraus tatsaechlich einen Installieren-Dialog
// anbieten, haengt zusaetzlich von HTTPS ab, das dieses Geraet (lokales
// Netzwerk, kein Zertifikat) nicht hat -- iOS "Zum Home-Bildschirm" und
// Androids manuelles "Zum Startbildschirm hinzufuegen" funktionieren aber
// auch ohne den automatischen Installations-Prompt und nutzen dieselben Icons.
static const char MANIFEST_JSON[] PROGMEM = R"JSON({
"name":"Pixel Status","short_name":"Pixel Status","start_url":"/","display":"standalone",
"background_color":"#0f172a","theme_color":"#0f172a",
"icons":[
{"src":"/icon-192.png","sizes":"192x192","type":"image/png"},
{"src":"/icon-512.png","sizes":"512x512","type":"image/png"}
]})JSON";

WebPortal::WebPortal(DisplayManager& display, NetManager& net, MqttBridge& mqtt,
                     LedController& leds, TimeManager& time, BatteryMonitor& battery,
                     uint16_t port)
  : _server(port), _display(display), _net(net), _mqtt(mqtt), _leds(leds), _time(time),
    _battery(battery) {}

// Default-Darstellungsfarben (LED 1 rot, LED 2 gruen, Matrix rot).
static const char* DEF_LED1   = "#ef4444";
static const char* DEF_LED2   = "#22c55e";
static const char* DEF_MATRIX = "#ff3b30";

// Nur "#rrggbb" akzeptieren -> keine Injection ueber die Farbfelder.
static bool isHexColor(const String& s) {
  if (s.length() != 7 || s[0] != '#') return false;
  for (uint8_t i = 1; i < 7; i++)
    if (!isxdigit((unsigned char)s[i])) return false;
  return true;
}

void WebPortal::loadAppearance() {
  _colLed1 = DEF_LED1; _colLed2 = DEF_LED2; _colMatrix = DEF_MATRIX;
  _name = "Pixel Status";
  File f = LittleFS.open("/ui.txt", "r");
  if (!f) return;
  auto line = [&]() { String s = f.readStringUntil('\n'); s.trim(); return s; };
  String a = line(), b = line(), c = line(), d = line();
  if (isHexColor(a)) _colLed1 = a;
  if (isHexColor(b)) _colLed2 = b;
  if (isHexColor(c)) _colMatrix = c;
  if (d.length())    _name = d;
  f.close();
}

// Nur gueltige Werte uebernehmen; leere/ungueltige lassen den Bestand unveraendert.
// Schreibt immer alle vier Zeilen (inkl. Anzeigename), damit Teil-Speicherungen
// die jeweils anderen Werte nicht verlieren.
bool WebPortal::saveAppearance(const String& led1, const String& led2, const String& matrix) {
  if (isHexColor(led1))   _colLed1   = led1;
  if (isHexColor(led2))   _colLed2   = led2;
  if (isHexColor(matrix)) _colMatrix = matrix;
  File f = LittleFS.open("/ui.txt", "w");
  if (!f) return false;
  f.print(_colLed1);   f.print('\n');
  f.print(_colLed2);   f.print('\n');
  f.print(_colMatrix); f.print('\n');
  f.print(_name);      f.print('\n');
  f.close();
  return true;
}

// Anzeigename bereinigen (Zeilenformat schuetzen, Laenge begrenzen) und speichern.
bool WebPortal::saveDisplayName(const String& name) {
  String n = name;
  n.replace("\r", ""); n.replace("\n", "");
  if (n.length() > 40) n = n.substring(0, 40);
  _name = n.length() ? n : String("Pixel Status");
  return saveAppearance(_colLed1, _colLed2, _colMatrix);   // schreibt inkl. _name
}

void WebPortal::begin() {
  LittleFS.begin();          // idempotent
  loadAppearance();
  // Sec-Fetch-Site muss explizit angefordert werden, sonst verwirft
  // ESP8266WebServer alle Header ausser Authorization/ETag (siehe isCrossSite()).
  _server.collectHeaders("Sec-Fetch-Site");
  setupRoutes();
  _server.begin();
}

void WebPortal::loop() { _server.handleClient(); }

// CSRF-Schutz (Blind-CSRF ueber fremde Webseiten, z. B. <img src="/wifi/save?...">):
// Sec-Fetch-Site ist ein Fetch-Metadata-Header, den alle aktuellen Browser bei
// JEDER Anfrage automatisch mitschicken -- auch bei <img>/<form>-GETs, wo
// Origin/Referer oft fehlen. "cross-site" bedeutet: eine andere Website hat die
// Anfrage ausgeloest, nicht die eigene Web-UI. Fehlt der Header (Companion-App
// via reqwest, curl, sehr alte Browser), wird die Anfrage weiterhin zugelassen --
// das Geraet vertraut wie bisher jedem Client im LAN, nur automatisiertes
// Cross-Site-Ausloesen aus fremden Webseiten wird verhindert.
bool WebPortal::isCrossSite() const {
  return _server.header("Sec-Fetch-Site") == "cross-site";
}

void WebPortal::onGuarded(const char* uri, std::function<void()> handler) {
  _server.on(uri, [this, handler]() {
    if (isCrossSite()) { _server.send(403, "text/plain", "Blocked: cross-site request"); return; }
    handler();
  });
}

// Minimales JSON-String-Escaping fuer SSIDs (Anfuehrungszeichen/Backslash).
static String jsonEscape(const String& in) {
  String out;
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if (c >= 0x20)        { out += c; }
  }
  return out;
}

// ---- Query-String-Parsing (fuer den USB-Weg: Befehlswert = "k1=v1&k2=v2") ----
static String urlDecode(const String& s) {
  auto hex = [](char h) -> int {
    if (h >= '0' && h <= '9') return h - '0';
    if (h >= 'a' && h <= 'f') return h - 'a' + 10;
    if (h >= 'A' && h <= 'F') return h - 'A' + 10;
    return -1;
  };
  String out; out.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '%' && i + 2 < s.length()) {
      int hi = hex(s[i + 1]), lo = hex(s[i + 2]);
      if (hi >= 0 && lo >= 0) { out += (char)(hi * 16 + lo); i += 2; continue; }
    }
    out += c;
  }
  return out;
}
static String queryArg(const String& query, const String& key) {
  String pref = key + "=";
  int i = 0;
  while (i <= (int)query.length()) {
    int amp = query.indexOf('&', i);
    int end = (amp < 0) ? query.length() : amp;
    String pair = query.substring(i, end);
    if (pair.startsWith(pref)) return urlDecode(pair.substring(pref.length()));
    if (amp < 0) break;
    i = amp + 1;
  }
  return "";
}
static bool queryHas(const String& query, const String& key) {
  String pref = key + "=";
  int i = 0;
  while (i <= (int)query.length()) {
    int amp = query.indexOf('&', i);
    int end = (amp < 0) ? query.length() : amp;
    if (query.substring(i, end).startsWith(pref)) return true;
    if (amp < 0) break;
    i = amp + 1;
  }
  return false;
}

// ---- Gemeinsame Status-JSON-Builder ----
String WebPortal::buildLedStatus() {
  return String("{\"autoWarn\":") + (_leds.autoWarn() ? "true" : "false") +
         ",\"warnSecs\":" + String(_leds.warnSecs()) +
         ",\"warnFastSecs\":" + String(_leds.warnFastSecs()) +
         ",\"activeLow\":" + (_leds.activeLow() ? "true" : "false") +
         ",\"alternate\":" + (_leds.alternate() ? "true" : "false") +
         ",\"blinkPeriod\":" + String(_leds.blinkPeriod()) +
         ",\"ledBrightness\":" + String(_leds.brightness()) +
         ",\"warn\":" + (_leds.warnActive() ? "true" : "false") +
         ",\"pin1\":" + String(_leds.pin1()) +
         ",\"pin2\":" + String(_leds.pin2()) + "}";
}
String WebPortal::buildAppearanceJson() {
  return "{\"led1\":\"" + _colLed1 + "\",\"led2\":\"" + _colLed2 +
         "\",\"matrix\":\"" + _colMatrix + "\",\"name\":\"" + jsonEscape(_name) + "\"}";
}
String WebPortal::buildSystemJson() {
  time_t now = time(nullptr);
  char ts[20]; struct tm tmv; localtime_r(&now, &tmv);
  strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M", &tmv);   // Format fuer <input type=datetime-local>
  return "{\"hostname\":\"" + jsonEscape(_net.hostname()) +
         "\",\"name\":\"" + jsonEscape(_name) + "\"" +
         ",\"ntpEnabled\":" + (_time.enabled() ? "true" : "false") +
         ",\"ntpServer\":\"" + jsonEscape(_time.server()) + "\"" +
         ",\"time\":\"" + String(ts) + "\"" +
         ",\"synced\":" + (TimeManager::synced() ? "true" : "false") +
         ",\"bootAnim\":" + String(_display.bootAnimation()) + "}";
}
String WebPortal::buildMqttStatus() {
  return String("{\"enabled\":") + (_mqtt.enabled() ? "true" : "false") +
         ",\"host\":\"" + jsonEscape(_mqtt.host()) + "\"" +
         ",\"port\":" + String(_mqtt.port()) +
         ",\"user\":\"" + jsonEscape(_mqtt.user()) + "\"" +
         ",\"base\":\"" + jsonEscape(_mqtt.base()) + "\"" +
         ",\"connected\":" + (_mqtt.connected() ? "true" : "false") + "}";
}
String WebPortal::buildWifiStatus() {
  bool conn = WiFi.status() == WL_CONNECTED;
  return String("{\"connected\":") + (conn ? "true" : "false") +
         ",\"ssid\":\"" + jsonEscape(WiFi.SSID()) + "\"" +
         ",\"ip\":\"" + WiFi.localIP().toString() + "\"" +
         ",\"hostname\":\"" + jsonEscape(_net.hostname()) + "\"" +
         ",\"ap\":" + (_net.apActive() ? "true" : "false") + "}";
}
String WebPortal::buildBatteryJson() {
  String out = String("{\"enabled\":") + (_battery.enabled() ? "true" : "false");
  if (_battery.enabled()) {
    out += ",\"voltage\":" + String(_battery.voltage(), 2);
    out += ",\"percent\":" + String(_battery.percent());
  }
  out += "}";
  return out;
}

// ---- Gemeinsame Apply-Logik ----
void WebPortal::applyLedConfig(const Arg& get) {
  _leds.saveConfig(get("autoWarn") == "1", get("warnSecs").toInt(),
                   get("warnFastSecs").toInt(), get("activeLow") == "1",
                   get("alternate") == "1", (uint16_t)get("blinkPeriod").toInt(),
                   (uint8_t)get("ledBrightness").toInt());
  saveAppearance(get("led1"), get("led2"), get("matrix"));  // ungueltige/leere bleiben unveraendert
}
bool WebPortal::applySystemConfig(const Arg& get, const Has& has) {
  if (has("name")) saveDisplayName(get("name"));
  // NTP: An/Aus + Server (sofort wirksam, kein Neustart). Leerer Server -> alter bleibt.
  if (has("ntpEnabled") || has("ntpServer"))
    _time.saveConfig(get("ntpEnabled") == "1", get("ntpServer"));
  // Uhrzeit manuell setzen (Epoch-Sekunden UTC); nach der NTP-Konfig, damit ein
  // gerade deaktiviertes NTP die Zeit nicht ueberschreibt.
  if (has("settime")) {
    long ep = get("settime").toInt();
    if (ep > 0) TimeManager::setManual((time_t)ep);
  }
  if (has("host")) {
    String h = get("host"); h.trim();
    if (h.length() && h != _net.hostname()) { _net.saveHostname(h); return true; }
  }
  return false;
}
bool WebPortal::applyWifiConfig(const Arg& get, const Has& has) {
  String host = has("host") ? get("host") : _net.hostname();
  _net.save(get("ssid"), get("pass"), host);
  return true;   // WiFi-Aenderung -> Neustart
}
bool WebPortal::applyMqttConfig(const Arg& get, const Has& has) {
  bool changePass = has("pass") && get("pass").length() > 0;
  _mqtt.saveConfig(get("enabled") == "1", get("host"), (uint16_t)get("port").toInt(),
                   get("user"), get("pass"), changePass, get("base"));
  return true;   // -> Neustart
}
// Kalibriert den A0-Spannungsteiler anhand einer mit dem Multimeter direkt an
// OUT+/OUT- gemessenen echten Spannung (siehe Battery.h). Kein Neustart noetig.
bool WebPortal::applyBatteryCalibration(const Arg& get, const Has& has) {
  if (!has("volts")) return false;
  float volts = get("volts").toFloat();
  return _battery.calibrate(volts);
}

// Transportunabhaengige Konfig-/Status-Befehle. `value` ist ein Query-String
// ("k1=v1&k2=v2", URL-kodiert). Wird von /api/cmd und der SerialBridge genutzt.
bool WebPortal::handleConfigCommand(const String& action, const String& value,
                                    String& reply, bool& restart) {
  restart = false;
  Arg get = [&value](const String& k) { return queryArg(value, k); };
  Has has = [&value](const String& k) { return queryHas(value, k); };
  if      (action == "getled")    reply = buildLedStatus();
  else if (action == "getappear") reply = buildAppearanceJson();
  else if (action == "getsys")    reply = buildSystemJson();
  else if (action == "getmqtt")   reply = buildMqttStatus();
  else if (action == "getwifi")   reply = buildWifiStatus();
  else if (action == "getbatt")   reply = buildBatteryJson();
  else if (action == "cfgled")  { applyLedConfig(get); reply = "{\"saved\":true}"; }
  else if (action == "cfgsys")  { restart = applySystemConfig(get, has);
                                  reply = String("{\"saved\":true,\"restart\":") +
                                          (restart ? "true" : "false") + "}"; }
  else if (action == "cfgwifi") { restart = applyWifiConfig(get, has); reply = "{\"saved\":true}"; }
  else if (action == "cfgmqtt") { restart = applyMqttConfig(get, has); reply = "{\"saved\":true}"; }
  else if (action == "cfgbattcal") {
    bool ok = applyBatteryCalibration(get, has);
    reply = String("{\"saved\":") + (ok ? "true" : "false") + "}";
  }
  else return false;
  return true;
}

void WebPortal::setupRoutes() {
  // Seiten immer ohne Cache ausliefern, damit der Browser nach einem Update nie
  // eine veraltete Version behaelt (sonst zeigen z. B. Einstellungsfelder Altwerte).
  auto page = [this](const char* body) {
    _server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    _server.send_P(200, "text/html", body);
  };
  _server.on("/", [this, page]() { page(PAGE); });
  _server.on("/style.css", [this]() {
    _server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    _server.send_P(200, "text/css", CSS);
  });
  _server.on("/lang.js", [this]() {
    _server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    _server.send_P(200, "application/javascript", LANG_JS);
  });
  _server.on("/health", [this, page]() { page(HEALTH_PAGE); });
  _server.on("/play", [this, page]() { page(PLAY_PAGE); });

  // Favicon/PWA-Icons (siehe Icons.h) + Manifest. Binaerdaten -- send_P()
  // braucht hier die explizite Laenge, PNG/ICO koennen eingebettete
  // Nullbytes enthalten (strlen_P wie bei den Text-Seiten waere falsch).
  auto icon = [this](const char* type, const uint8_t* data, size_t len) {
    _server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    _server.send_P(200, type, (PGM_P)data, len);
  };
  _server.on("/favicon.ico",         [this, icon]() { icon("image/x-icon", FAVICON_ICO,           FAVICON_ICO_LEN); });
  _server.on("/favicon.png",         [this, icon]() { icon("image/png",    FAVICON_PNG,            FAVICON_PNG_LEN); });
  _server.on("/apple-touch-icon.png",[this, icon]() { icon("image/png",    APPLE_TOUCH_ICON_PNG,   APPLE_TOUCH_ICON_PNG_LEN); });
  _server.on("/icon-192.png",        [this, icon]() { icon("image/png",    ICON_192_PNG,           ICON_192_PNG_LEN); });
  _server.on("/icon-512.png",        [this, icon]() { icon("image/png",    ICON_512_PNG,           ICON_512_PNG_LEN); });
  _server.on("/manifest.json", [this]() {
    _server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    _server.send_P(200, "application/manifest+json", MANIFEST_JSON);
  });

  // Generischer Endpunkt fuer die Companion-App: /api/cmd?action=...&value=...
  // Behandelt sowohl Steuer- (runCommand) als auch Konfig-Befehle (get*/cfg*),
  // damit die App ueber HTTP und USB dasselbe Protokoll nutzt.
  onGuarded("/api/cmd", [this]() {
    String action = _server.arg("action"), value = _server.arg("value");
    String reply; bool restart = false;
    if (handleConfigCommand(action, value, reply, restart)) {
      _server.send(200, "application/json", reply);
      if (restart) { delay(400); ESP.restart(); }
    } else {
      runCommand(_display, _leds, action, value);
      _server.send(200, "application/json", combinedStateJson(_display, _leds));
    }
  });

  // Komfort-Routen fuer die eingebaute Web-UI.
  onGuarded("/api/text", [this]() {
    runCommand(_display, _leds,_server.arg("scroll") == "1" ? "scroll" : "text", _server.arg("msg"));
    _server.send(200, "application/json", _display.stateJson());
  });

  onGuarded("/api/preset", [this]() {
    runCommand(_display, _leds,"preset", _server.arg("name"));
    _server.send(200, "application/json", _display.stateJson());
  });

  onGuarded("/api/timer", [this]() {
    String value = _server.hasArg("stop")   ? "stop"
                 : _server.hasArg("pause")  ? "pause"
                 : _server.hasArg("resume") ? "resume"
                 : (_server.arg("dir") == "up" ? "up" : _server.arg("seconds"));
    runCommand(_display, _leds,"timer", value);
    _server.send(200, "application/json", _display.stateJson());
  });

  onGuarded("/api/game", [this]() {
    runCommand(_display, _leds, "game", _server.arg("cmd"));
    _server.send(200, "application/json", _display.stateJson());
  });

  onGuarded("/api/brightness", [this]() {
    runCommand(_display, _leds,"brightness", _server.arg("level"));
    _server.send(200, "application/json", _display.stateJson());
  });

  onGuarded("/api/ledbrightness", [this]() {
    runCommand(_display, _leds,"ledbrightness", _server.arg("level"));
    _server.send(200, "application/json", buildLedStatus());
  });

  onGuarded("/api/orientation", [this]() {
    runCommand(_display, _leds,"orient", _server.arg("deg"));
    _server.send(200, "application/json", _display.stateJson());
  });

  onGuarded("/api/scrolldir", [this]() {
    runCommand(_display, _leds,"scrolldir", _server.arg("dir"));
    _server.send(200, "application/json", _display.stateJson());
  });

  onGuarded("/api/bootanim", [this]() {
    runCommand(_display, _leds,"bootanim", _server.arg("type"));
    _server.send(200, "application/json", _display.stateJson());
  });

  onGuarded("/api/clear", [this]() {
    runCommand(_display, _leds,"clear", "");
    _server.send(200, "application/json", _display.stateJson());
  });

  _server.on("/api/state", [this]() {
    _server.send(200, "application/json", combinedStateJson(_display, _leds));
  });

  // Zusatz-LEDs schalten: /api/led?i=1|2|both&s=off|on|blink
  onGuarded("/api/led", [this]() {
    if (_server.hasArg("s"))
      runCommand(_display, _leds, "led", _server.arg("i") + " " + _server.arg("s"));
    _server.send(200, "application/json", combinedStateJson(_display, _leds));
  });

  // Aktueller Pixelpuffer (32 Spaltenbytes) fuer die Live-Vorschau.
  _server.on("/api/frame", [this]() {
    _server.send(200, "application/json", _display.frameJson());
  });

  // Laufzeit-Diagnose: freier Heap, Fragmentierung, Uptime, WiFi, Flash/FS.
  _server.on("/api/health", [this]() {
    FSInfo fi; LittleFS.info(fi);
    String out = "{";
    out += "\"heap\":"       + String(ESP.getFreeHeap());
    out += ",\"frag\":"      + String(ESP.getHeapFragmentation());
    out += ",\"maxblock\":"  + String(ESP.getMaxFreeBlockSize());
    out += ",\"cpu\":"       + String(LoopStats::load());
    out += ",\"lps\":"       + String(LoopStats::loopsPerSec());
    out += ",\"uptime\":"    + String(millis() / 1000);
    out += ",\"rssi\":"      + String(WiFi.isConnected() ? WiFi.RSSI() : 0);
    out += ",\"sketch\":"    + String(ESP.getSketchSize());
    out += ",\"sketchfree\":"+ String(ESP.getFreeSketchSpace());
    out += ",\"fsused\":"    + String(fi.usedBytes);
    out += ",\"fstotal\":"   + String(fi.totalBytes);
    time_t now = time(nullptr);
    char ts[24]; struct tm tmv; localtime_r(&now, &tmv);
    strftime(ts, sizeof(ts), "%d.%m.%Y %H:%M:%S", &tmv);
    out += ",\"time\":\""    + String(ts) + "\"";
    out += ",\"ntp\":"       + String(now > 1600000000 ? "true" : "false");  // Zeit nach ~2020 = NTP ok
    out += ",\"battEnabled\":" + String(_battery.enabled() ? "true" : "false");
    if (_battery.enabled()) {
      out += ",\"battVoltage\":" + String(_battery.voltage(), 2);
      out += ",\"battPct\":"     + String(_battery.percent());
      out += ",\"battLow\":"     + String(_battery.low() ? "true" : "false");
    }
    out += "}";
    _server.send(200, "application/json", out);
  });

  // Weitgehend statische System-Infos (einmalig beim Laden geholt). Im
  // Setup-Hotspot-Fallback (siehe NetManager::startAP()) sind die STA-Felder
  // (WiFi.localIP()/gatewayIP()/SSID()/...) leer/0 -- das Geraet ist ja nicht
  // als Client verbunden, sondern selbst der Access Point. apMode==true liefert
  // stattdessen die AP-eigenen Werte (softAPIP()/softAPmacAddress()/apSsid()),
  // damit /health auch im Fallback-Modus etwas Sinnvolles anzeigt.
  _server.on("/api/sysinfo", [this]() {
    bool ap = _net.apActive();
    String o = "{";
    o += "\"apMode\":"     + String(ap ? "true" : "false");
    if (ap) {
      o += ",\"ip\":\""    + WiFi.softAPIP().toString() + "\"";
      o += ",\"mac\":\""   + WiFi.softAPmacAddress() + "\"";
      o += ",\"ssid\":\""  + jsonEscape(_net.apSsid()) + "\"";
      o += ",\"clients\":" + String(WiFi.softAPgetStationNum());
    } else {
      o += ",\"ip\":\""    + WiFi.localIP().toString() + "\"";
      o += ",\"gw\":\""    + WiFi.gatewayIP().toString() + "\"";
      o += ",\"mask\":\""  + WiFi.subnetMask().toString() + "\"";
      o += ",\"dns\":\""   + WiFi.dnsIP().toString() + "\"";
      o += ",\"mac\":\""   + WiFi.macAddress() + "\"";
      o += ",\"host\":\""  + jsonEscape(_net.hostname()) + "\"";
      o += ",\"ssid\":\""  + jsonEscape(WiFi.SSID()) + "\"";
      o += ",\"bssid\":\"" + WiFi.BSSIDstr() + "\"";
      o += ",\"ch\":"      + String(WiFi.channel());
    }
    o += ",\"sdk\":\""     + String(ESP.getSdkVersion()) + "\"";
    o += ",\"core\":\""    + ESP.getCoreVersion() + "\"";
    o += ",\"cpu\":"       + String(ESP.getCpuFreqMHz());
    o += ",\"chip\":\""    + String(ESP.getChipId(), HEX) + "\"";
    o += ",\"build\":\""   + String(BUILD_TS) + "\"";
    o += ",\"flashreal\":" + String(ESP.getFlashChipRealSize());
    o += ",\"flashconf\":" + String(ESP.getFlashChipSize());
    o += "}";
    _server.send(200, "application/json", o);
  });

  // ---- WiFi-Einrichtung ----
  _server.on("/wifi", [this, page]() { page(SETTINGS_PAGE); });

  // Scan anstossen (nicht-blockierend, FUNC-03): WiFi.scanNetworks() blockierte
  // frueher den kompletten loop() fuer mehrere Sekunden -- LEDs, Timer, MQTT
  // standen waehrenddessen still. scanNetworksAsync() laesst den Scan im
  // Hintergrund laufen, die Route kehrt sofort zurueck; das Ergebnis wird ueber
  // /wifi/scan/result gepollt. WIFI_SCAN_RUNNING (-1) heisst: schon ein Scan
  // aktiv -> keinen zweiten anstossen, sonst wuerde der Client neu starten.
  onGuarded("/wifi/scan", [this]() {
    if (WiFi.scanComplete() != WIFI_SCAN_RUNNING) WiFi.scanNetworksAsync([](int) {});
    _server.send(200, "application/json", "{\"status\":\"started\"}");
  });

  // Wird gepollt, bis der in /wifi/scan angestossene Scan fertig ist. Rein
  // lesend (kein onGuarded noetig, wie die anderen */status-Routen).
  _server.on("/wifi/scan/result", [this]() {
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) { _server.send(200, "application/json", "{\"status\":\"running\"}"); return; }
    if (n == WIFI_SCAN_FAILED)  { _server.send(200, "application/json", "{\"status\":\"failed\"}");  return; }
    String out = "{\"status\":\"done\",\"networks\":[";
    for (int i = 0; i < n; i++) {
      if (i) out += ',';
      out += "{\"ssid\":\"" + jsonEscape(WiFi.SSID(i)) + "\",\"rssi\":" + WiFi.RSSI(i) + "}";
    }
    out += "]}";
    WiFi.scanDelete();
    _server.send(200, "application/json", out);
  });

  // Aktueller Verbindungsstatus + Hostname (fuers Vorbefuellen der Seite).
  // Die folgenden Status-/Save-Routen teilen sich die Logik mit den Companion-
  // Befehlen (get*/cfg*) ueber die gemeinsamen build*/apply*-Helfer.
  _server.on("/wifi/status", [this]() {
    _server.send(200, "application/json", buildWifiStatus());
  });

  // Zugangsdaten speichern und neu starten. Hostname bleibt (System-Tab).
  onGuarded("/wifi/save", [this]() {
    Arg get = [this](const String& k) { return _server.arg(k); };
    Has has = [this](const String& k) { return _server.hasArg(k); };
    applyWifiConfig(get, has);
    _server.send(200, "application/json", "{\"saved\":true}");
    delay(400); ESP.restart();
  });

  // ---- System-Einstellungen (Hostname + Anzeigename) ----
  _server.on("/system/status", [this]() {
    _server.send(200, "application/json", buildSystemJson());
  });

  onGuarded("/system/save", [this]() {
    Arg get = [this](const String& k) { return _server.arg(k); };
    Has has = [this](const String& k) { return _server.hasArg(k); };
    bool restart = applySystemConfig(get, has);
    _server.send(200, "application/json",
                 String("{\"saved\":true,\"restart\":") + (restart ? "true" : "false") + "}");
    if (restart) { delay(400); ESP.restart(); }
  });

  // ---- Akku-Kalibrierung (Teil des System-Tabs) ----
  _server.on("/battery/status", [this]() {
    _server.send(200, "application/json", buildBatteryJson());
  });
  onGuarded("/battery/calibrate", [this]() {
    Arg get = [this](const String& k) { return _server.arg(k); };
    Has has = [this](const String& k) { return _server.hasArg(k); };
    bool ok = applyBatteryCalibration(get, has);
    _server.send(200, "application/json", String("{\"saved\":") + (ok ? "true" : "false") + "}");
  });

  // ---- MQTT-Einrichtung ----
  _server.on("/mqtt", [this, page]() { page(SETTINGS_PAGE); });

  _server.on("/mqtt/status", [this]() {
    _server.send(200, "application/json", buildMqttStatus());
  });

  onGuarded("/mqtt/save", [this]() {
    Arg get = [this](const String& k) { return _server.arg(k); };
    Has has = [this](const String& k) { return _server.hasArg(k); };
    applyMqttConfig(get, has);
    _server.send(200, "application/json", "{\"saved\":true}");
    delay(400); ESP.restart();
  });

  // ---- Einstellungen der Zusatz-LEDs ----
  _server.on("/leds", [this, page]() { page(SETTINGS_PAGE); });
  _server.on("/settings", [this, page]() { page(SETTINGS_PAGE); });

  _server.on("/leds/status", [this]() {
    _server.send(200, "application/json", buildLedStatus());
  });

  _server.on("/api/appearance", [this]() {
    _server.send(200, "application/json", buildAppearanceJson());
  });

  // Speichern + sofort anwenden (kein Neustart noetig).
  onGuarded("/leds/save", [this]() {
    Arg get = [this](const String& k) { return _server.arg(k); };
    applyLedConfig(get);
    _server.send(200, "application/json", "{\"saved\":true}");
  });

  _server.onNotFound([this]() { _server.send(404, "text/plain", "Not found"); });
}
