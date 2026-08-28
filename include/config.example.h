#pragma once
// Vorlage. Kopieren nach include/config.h und ausfuellen:
//   cp include/config.example.h include/config.h
// config.h ist per .gitignore ausgeschlossen (enthaelt Zugangsdaten).

// ===== WiFi =====
#define WIFI_SSID       "DEIN_WLAN"          // Default; per Web-Portal ueberschreibbar
#define WIFI_PASSWORD   "DEIN_PASSWORT"
#define HOSTNAME        "pixelstatus"        // Default; im Web-Portal /wifi aenderbar (-> <name>.local)

// ===== Setup-Hotspot (Fallback) =====
// Bekommt das Display keine WiFi-Verbindung, oeffnet es diesen Access Point.
// Verbinde dich damit und rufe http://192.168.4.1/wifi auf, um SSID/Passwort
// einzutragen (wird im Flash gespeichert, kein Neu-Flashen noetig).
#define AP_SSID         "pixelstatus-control"
// Leer lassen (empfohlen): Passwort wird automatisch und geraetespezifisch aus
// der Chip-ID generiert (siehe NetManager::startAP()) und beim Einschalten
// abwechselnd mit dem AP-Namen auf der Matrix angezeigt, bis sich jemand
// verbindet. Eigenes Passwort (>=8 Zeichen, WPA2-Minimum) ueberschreibt das.
#define AP_PASSWORD     ""

// ===== MQTT (optional, fuer Standalone-Betrieb nicht noetig) =====
// Diese Werte sind nur DEFAULTS. MQTT laesst sich zur Laufzeit im Web-Portal
// unter http://<host>/mqtt einrichten (Host, Port, User, Passwort, Topic) und
// wird im Flash gespeichert - kein Neu-Flashen noetig.
#define MQTT_ENABLED    false                // Default aus; per /mqtt aktivierbar
#define MQTT_HOST       "192.168.1.10"
#define MQTT_PORT       1883
#define MQTT_USER       ""                   // leer lassen = ohne Auth
#define MQTT_PASSWORD   ""
#define MQTT_BASE_TOPIC "pixelstatus"        // -> pixelstatus/cmd/<aktion>, pixelstatus/state

// ===== Hardware =====
#define MAX_DEVICES        4                 // Anzahl der 8x8-Module
#define CS_PIN             15                // D8 / GPIO15  (CS frei waehlbar)
#define DEFAULT_BRIGHTNESS 4                 // 0..15
// HW-SPI fest: DIN = D7 / GPIO13 (MOSI), CLK = D5 / GPIO14 (SCK)

// ===== Zusatz-LEDs (zwei separat schaltbare Status-LEDs) =====
// Zwei einfache LEDs (mit Vorwiderstand) an freien GPIOs. Getrennt per
// 'led'-Befehl schaltbar (Web/MQTT/USB) und mit automatischer Countdown-
// Warnung: in den letzten Minuten eines Timer-Countdowns blinken beide LEDs.
#define LED_PIN_1          4     // D2 / GPIO4  -> LED 1 = ROT (frei, ohne Boot-Nebenwirkungen)
#define LED_PIN_2          5     // D1 / GPIO5  -> LED 2 = GRUEN
#define LED_ACTIVE_LOW     false // true, falls LED gegen 3V3 verdrahtet (LOW = an)
#define LED_WARN_SECS      300   // Countdown-Warnung: ab hier blinken beide (letzte 5 Min)
#define LED_WARN_FAST_SECS 60    // ab hier schnell blinken (letzte Minute); 0 = aus

// ===== Uhrzeit (NTP, nur mit WiFi) =====
// NTP_SERVER ist nur der Default: Server + An/Aus sind zur Laufzeit ueber
// /settings (System-Tab) einstellbar und liegen in LittleFS (/ntp.txt).
// Die Zeitzone (NTP_TZ) bleibt compile-time.
#define NTP_SERVER  "pool.ntp.org"
#define NTP_TZ      "CET-1CEST,M3.5.0,M10.5.0/3"   // TZ-String, hier Europa/Berlin

// ===== Akku-Ueberwachung (optional, 1S-LiPo ueber A0-Spannungsteiler) =====
// Erwartet den externen Teiler R1=100k (Akku OUT+ -> A0) / R2=220k (A0 -> GND)
// + 100nF an A0, siehe hardware/wiring/parts-list.md. Ohne diesen Teiler
// liefert A0 nur Rauschen -> deshalb standardmaessig AUS, erst nach dem
// Verloeten der Akku-Hardware aktivieren.
#define BATTERY_MONITOR_ENABLED false
#define BATTERY_EMPTY_V         3.3f   // Akkuspannung bei 0 %
#define BATTERY_FULL_V          4.2f   // Akkuspannung bei 100 % (voller 1S-LiPo)
#define BATTERY_LOW_PERCENT     15     // ab hier: "LOW BATT" auf der Matrix + Helligkeit gedrosselt
#define BATTERY_LOW_BRIGHTNESS  1      // gedrosselte Helligkeit (0..15) im Low-Battery-Zustand
