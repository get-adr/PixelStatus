#include "DisplayManager.h"
#include <time.h>
#include <LittleFS.h>
#include <string.h>

// Gespeicherte Helligkeit (ueberlebt Neustarts).
static const char* BRIGHT_PATH = "/bright.txt";
static uint8_t loadBrightness(uint8_t def) {
  File f = LittleFS.open(BRIGHT_PATH, "r");
  if (!f) return def;
  long v = f.parseInt();
  f.close();
  return (v >= 0 && v <= 15) ? (uint8_t)v : def;
}

// Gespeicherte Ausrichtung (ueberlebt Neustarts).
static const char* ORIENT_PATH = "/orient.txt";
static uint16_t loadOrientation() {
  File f = LittleFS.open(ORIENT_PATH, "r");
  if (!f) return 0;
  long v = f.parseInt();
  f.close();
  return (v == 180) ? (uint16_t)v : 0;
}

// Gespeicherte Scrollrichtung (ueberlebt Neustarts).
static const char* SCROLLDIR_PATH = "/scrolldir.txt";
static bool loadScrollReverse() {
  File f = LittleFS.open(SCROLLDIR_PATH, "r");
  if (!f) return false;
  long v = f.parseInt();
  f.close();
  return v == 1;
}

// Gespeicherter Top-Score des Space-Invaders-Gimmicks (ueberlebt Neustarts,
// siehe gameOverEnd()).
static const char* TOPSCORE_PATH = "/topscore.txt";
static uint16_t loadTopScore() {
  File f = LittleFS.open(TOPSCORE_PATH, "r");
  if (!f) return 0;
  long v = f.parseInt();
  f.close();
  return (v >= 0 && v <= 65535) ? (uint16_t)v : 0;
}

// Kehrt die Bitreihenfolge eines Bytes um (Bit 0 <-> Bit 7 usw.) -- fuer die
// 90/180-Grad-Rotation (siehe renderRotated()).
static uint8_t reverseBits8(uint8_t v) {
  v = (uint8_t)((v & 0xF0) >> 4 | (v & 0x0F) << 4);
  v = (uint8_t)((v & 0xCC) >> 2 | (v & 0x33) << 2);
  v = (uint8_t)((v & 0xAA) >> 1 | (v & 0x55) << 1);
  return v;
}

// Gespeicherte Einschalt-Animation (ueberlebt Neustarts).
static const char* BOOTANIM_PATH = "/bootanim.txt";
static uint8_t loadBootAnim() {
  File f = LittleFS.open(BOOTANIM_PATH, "r");
  if (!f) return DisplayManager::BOOT_SCAN;   // Default fuer neue/aktualisierte Geraete
  long v = f.parseInt();
  f.close();
  return (v >= 0 && v <= 3) ? (uint8_t)v : DisplayManager::BOOT_SCAN;
}

// FC16_HW passt zu den verbreiteten 4-in-1 MAX7219-Modulen. Falls die Anzeige
// gespiegelt/verschoben erscheint, hier auf GENERIC_HW oder PAROLA_HW wechseln.
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW

// Die eingebaute Systemschrift laesst Zeile 7 (unterste Reihe) bei den meisten
// Zeichen frei (Buchstaben/Ziffern nutzen nur Zeile 0-6) -- Text "schwebt" dadurch
// eine Zeile ueber der unteren Kante statt auf ihr zu sitzen. Fix: einmalig beim
// Start eine Kopie der Schrift bauen, in der jedes Zeichen um 1 Zeile nach unten
// verschoben ist (Byte << 1, da Bit r = Zeile r). Zeichen mit Unterlaenge (g, j,
// p, q, y, ...) nutzen Zeile 7 bereits selbst -> die bleiben unverschoben, sonst
// wuerde ihr unterster Pixel abgeschnitten. Deckt den druckbaren ASCII-Bereich
// sowie Latin-1 (32-252, inkl. Ä/Ö/Ü/ß/ä/ö/ü) ab -- der Bereich MUSS laut
// MD_MAX72xx-Fontformat luecken-frei sein (first..last durchlaufend), daher auch
// die dazwischenliegenden, selten genutzten Codes; setFont() nimmt laut
// Bibliothek auch einen RAM-Zeiger (kein AVR-PROGMEM-Zwang auf dem ESP8266).
static uint8_t s_shiftedFont[1300];
static void buildShiftedFont(MD_MAX72XX* g) {
  const uint8_t first = 32, last = 252;
  uint16_t off = 0;
  s_shiftedFont[off++] = 'F';
  s_shiftedFont[off++] = 2;
  s_shiftedFont[off++] = 0; s_shiftedFont[off++] = first;
  s_shiftedFont[off++] = 0; s_shiftedFont[off++] = last;
  s_shiftedFont[off++] = 8;   // Zeichenhoehe
  for (int code = first; code <= last; code++) {
    uint8_t buf[16];
    uint8_t w = g->getChar((uint8_t)code, sizeof(buf), buf);
    if ((size_t)(off + w + 1) >= sizeof(s_shiftedFont)) break;   // Sicherheitsnetz
    bool hasDescender = false;
    for (uint8_t i = 0; i < w; i++) if (buf[i] & 0x80) { hasDescender = true; break; }
    uint8_t shift = hasDescender ? 0 : 1;
    s_shiftedFont[off++] = w;
    for (uint8_t i = 0; i < w; i++) s_shiftedFont[off++] = (uint8_t)(buf[i] << shift);
  }
}

// Web-UI/MQTT/USB liefern Text als UTF-8 (Browser/Rust-Strings); der Font
// indiziert Umlaute (Ä/Ö/Ü/ß/ä/ö/ü) aber als Einzelbyte-Codes nach Latin-1
// (siehe MD_MAX72xx_font.cpp, "USE_NEW_FONT"-Tabelle, Codes 196-252). Wandelt
// die zweibytigen UTF-8-Folgen der Latin-1-Zusatzzeichen (U+0080-U+00FF, Leitbyte
// 0xC2/0xC3) in das erwartete Einzelbyte um. Andere Mehrbyte-UTF-8-Folgen (Emoji,
// nicht-lateinische Schrift) hat der Font ohnehin nicht -> werden verworfen statt
// als Muell-Bytes gerendert.
static String utf8ToLatin1(const String& in) {
  String out; out.reserve(in.length());
  for (size_t i = 0; i < in.length(); i++) {
    uint8_t c = (uint8_t)in[i];
    if (c < 0x80) {
      out += (char)c;
    } else if ((c == 0xC2 || c == 0xC3) && i + 1 < in.length()) {
      uint8_t c2 = (uint8_t)in[i + 1];
      out += (char)(((c & 0x1F) << 6) | (c2 & 0x3F));
      i++;
    }
  }
  return out;
}

DisplayManager::DisplayManager(uint8_t csPin, uint8_t numDevices)
  : _p(HARDWARE_TYPE, csPin, numDevices), _numDevices(numDevices) {}

void DisplayManager::begin(uint8_t brightness) {
  _p.begin();
  LittleFS.begin();                            // idempotent; fuer gespeicherte Helligkeit/Ausrichtung
  _brightness = loadBrightness(brightness);     // gespeicherter Wert, sonst Default
  _p.setIntensity(_brightness);
  _orientation = loadOrientation();
  _scrollReverse = loadScrollReverse();
  _bootAnim = loadBootAnim();
  _gTopScore = loadTopScore();
  buildShiftedFont(_p.getGraphicObject());       // muss vor dem ersten getChar()-Aufruf stehen
  _p.setFont(s_shiftedFont);                     // gilt fuer Parola UND den eigenen Renderer (buildRotBuf nutzt dieselbe getChar()-API)
  _p.displayClear();
}

void DisplayManager::loop() {
  if (_animActive) {
    // Boot-Animationen schreiben direkt in denselben Puffer, den Parola/
    // renderRotated() verwalten -- wuerden sich also gegenseitig ueberschreiben,
    // liefen beide im selben Tick. Deshalb pausiert die normale Wiedergabe,
    // bis die Animation fertig ist.
    stepBootAnimation();
    return;
  }
  if (_mode == GAME) {
    // Schreibt wie die Boot-Animationen direkt per writeLogicalColumn() --
    // wuerde sich mit Parolas displayAnimate()/renderRotated() genauso ins
    // Gehege kommen, deshalb hier ebenfalls ein fruehes return.
    renderGame();
    return;
  }
  if (_mode == TIMER) renderTimer();
  else if (_mode == CLOCK) renderClock();

  if (_orientation == 0) {
    // IDLE hat nichts zu animieren -- ohne diese Bedingung wuerde
    // displayAnimate() nach clear() (z.B. "Aus") die zuvor konfigurierte
    // Scroll-Animation (etwa den laufenden GAME-OVER-Text) einfach weiter
    // abspielen und den gerade geleerten Puffer wieder ueberschreiben, da
    // displayClear() nur den Pixelpuffer leert, nicht Parolas laufende
    // Animation stoppt.
    if (_mode != IDLE && _p.displayAnimate()) {
      if (_mode == TEXT_SCROLL) _p.displayReset();  // Scrolltext endlos wiederholen
    }
  } else {
    renderRotated();
  }
}

// Maximale Laenge fuer benutzerdefinierten Text (SEC-06): ohne Grenze liessen
// sich beliebig lange Strings speichern -- auf dem ~50-KB-Heap des ESP8266
// ein Fragmentierungsrisiko, verstaerkt dadurch, dass _text bei jedem
// /api/state-Poll (jede Sekunde) sowie bei jeder MQTT-Statusaenderung erneut
// in stateJson() serialisiert wird. 128 Byte reichen fuer ein 32x8-Scrolldisplay
// bei weitem; wie saveDisplayName() (Anzeigename, 40 Zeichen) derselbe Ansatz.
static const size_t MAX_TEXT_LEN = 128;

void DisplayManager::setText(const String& text, bool scroll) {
  _p.displayClear();                // alten Inhalt entfernen -> sauberer Uebergang
  // Kuerzung VOR utf8ToLatin1(), damit _text (API-Rundreise) und _renderText
  // (tatsaechlich gerendert) konsistent aus derselben, begrenzten Fassung
  // stammen -- nicht der eine aus dem Original, der andere aus der Kuerzung.
  _text = (text.length() > MAX_TEXT_LEN) ? text.substring(0, MAX_TEXT_LEN) : text;
  _renderText = utf8ToLatin1(_text); // tatsaechlich gerenderte Fassung (Font-Codes)
  _mode = scroll ? TEXT_SCROLL : TEXT_STATIC;
  renderContent(_renderText, scroll);
}

// Tatsaechliche Pixelbreite laut Font (Zeichenbreiten + je 1 Spalte Abstand).
uint16_t DisplayManager::textWidth(const String& text) {
  MD_MAX72XX* g = _p.getGraphicObject();
  uint8_t buf[16];
  uint16_t w = 0;
  for (uint16_t i = 0; i < text.length(); i++)
    w += g->getChar((uint8_t)text[i], sizeof(buf), buf) + 1;  // +1 Zeichenabstand
  return w ? w - 1 : 0;                                       // letzter Abstand entfaellt
}

void DisplayManager::showMessage(const String& text) {
  // Scrollt nur, wenn der Text wirklich breiter als die Anzeige ist. Breite anhand
  // der gerenderten (Latin-1-gewandelten) Fassung -- Umlaute zaehlen sonst falsch.
  bool scroll = textWidth(utf8ToLatin1(text)) > (uint16_t)(_numDevices * 8);
  setText(text, scroll);
}

void DisplayManager::scrollMessage(const String& text) { setText(text, true); }

void DisplayManager::startTimer(long seconds, bool countdown) {
  _mode         = TIMER;
  _countdown    = countdown;
  _timerBase    = seconds;
  _timerStart   = millis();
  _timerPaused  = false;
  _lastShown    = -1;
  _p.displayClear();                // sauberer Uebergang von vorherigem Inhalt
  renderTimer();
}

// Friert die Anzeige ein: renderTimer() berechnet die verstrichene Zeit ueber
// millis()-_timerStart, die Restsekunden muessen daher VOR dem Setzen von
// _timerPaused ermittelt werden (timerRemaining() rechnet sonst live weiter).
void DisplayManager::pauseTimer() {
  if (_mode != TIMER || _timerPaused) return;
  _pausedRemaining = timerRemaining();
  _pauseStart       = millis();
  _timerPaused      = true;
}

// Verschiebt _timerStart um die Pausendauer nach vorn, damit die naechste
// Restzeit-Berechnung nahtlos dort weitermacht, wo pauseTimer() eingefroren hat.
void DisplayManager::resumeTimer() {
  if (_mode != TIMER || !_timerPaused) return;
  _timerStart  += (millis() - _pauseStart);
  _timerPaused  = false;
  _lastShown    = -1;               // erzwingt sofortiges Neuzeichnen
  renderTimer();
}

void DisplayManager::stopTimer() { _timerPaused = false; clear(); }

void DisplayManager::startClock() {
  _mode      = CLOCK;
  _lastShown = -1;                    // erzwingt sofortiges Zeichnen
  _lastClockCheck = millis() - 600;   // Drossel ueberspringen -> sofort rendern
  _p.displayClear();                  // sauberer Uebergang von vorherigem Inhalt
  renderClock();
}

void DisplayManager::clear() {
  _mode = IDLE;
  _text = "";
  _timerPaused = false;
  _p.displayClear();
  _rotLen = 0;                        // eigener Renderer zeigt nichts mehr an
  _rotDirty = true;                   // naechster renderRotated()-Aufruf loescht die Hardware
}

// Space-Invaders-Gimmick: reine Web-UI-Steuerung (siehe WebPortal /api/game).
// gameUp()/gameDown()/gameFire() sind bewusst duenne Setter/No-Ops ausserhalb
// GAME-Modus -- der naechste renderGame()-Tick (aus loop(), gedrosselt per
// millis()) uebernimmt Bewegung/Zeichnen, kein sofortiges Neuzeichnen hier noetig.
void DisplayManager::gameStart() {
  _mode  = GAME;
  _gDefY = 3;
  for (uint8_t i = 0; i < GAME_MAX_INV; i++)     _gInv[i].alive = false;
  for (uint8_t i = 0; i < GAME_MAX_BULLETS; i++) _gBullet[i].alive = false;
  _gScore   = 0;
  _gLives   = 3;
  _gBombs          = 2;
  _gBombFlashSteps = 0;
  _gBombFlashLed   = -1;
  _gLedsOwned      = true;
  _gLastTick = _gLastSpawn = millis();
  startGameIntro();
}

// Waehrend Intro/Titel (siehe startGameIntro()/startGameTitle()) reagieren
// die Steuerbefehle bewusst nicht -- sonst koennte z.B. ein zu frueh
// abgefeuerter Schuss unbewegt "einfrieren", bis das Gameplay tatsaechlich
// beginnt (die Bewegungs-/Zeichenlogik dafuer laeuft erst danach).
void DisplayManager::gameUp() {
  if (_mode != GAME || _gIntroActive || _gTitleActive) return;
  if (_gDefY > 0) _gDefY--;
}

void DisplayManager::gameDown() {
  if (_mode != GAME || _gIntroActive || _gTitleActive) return;
  if (_gDefY < 7) _gDefY++;
}

// Bis zu GAME_MAX_BULLETS gleichzeitig unterwegs -- ist die Kapazitaet voll,
// verpufft ein weiterer Feuerbefehl (natuerliche Rate-Begrenzung statt eines
// eigenen Cooldown-Timers).
void DisplayManager::gameFire() {
  if (_mode != GAME || _gIntroActive || _gTitleActive) return;
  uint16_t cols = (uint16_t)_numDevices * 8;
  for (uint8_t i = 0; i < GAME_MAX_BULLETS; i++) {
    if (!_gBullet[i].alive) {
      _gBullet[i].alive = true;
      _gBullet[i].x = (int16_t)cols - 2;   // eine Spalte links vom Verteidiger
      _gBullet[i].y = _gDefY;
      break;
    }
  }
}

// Zuendet eine der 2 Bomben (erste = LED 0, zweite = LED 1 -- main.cpp liest
// gameLedState() und steuert die zugehoerige Zusatz-LED darueber, siehe
// Klassenkommentar). Wirkung selbst kommt aus renderGame(): waehrend
// _gBombFlashSteps>0 blinken alle sichtbaren Invader ein paar Ticks lang und
// verschwinden dann komplett.
void DisplayManager::gameBomb() {
  if (_mode != GAME || _gIntroActive || _gTitleActive || _gBombs == 0) return;
  _gBombFlashLed = (int8_t)(2 - _gBombs);   // 2 verbleibend -> LED 0, 1 verbleibend -> LED 1
  _gBombs--;
  _gBombFlashSteps = 6;                     // 6 Ticks * 220ms ~ 1.3s, ~3 Blinkzyklen
}

// Liefert main.cpp den gewuenschten Zustand fuer LED idx (0/1): waehrend des
// Spiels zeigen beide Zusatz-LEDs per Ein/Aus an, ob die jeweilige Bombe noch
// verfuegbar ist -- die erste verbrauchte Bombe betrifft immer LED 0, die
// zweite LED 1 (dieselbe Zuordnung wie in gameBomb()). Verlaesst das Spiel
// GAME (Game Over oder "Aus"/clear()), sollen die LEDs nicht in ihrem letzten
// Bomben-Zustand haengen bleiben, sondern explizit ausgehen -- aber nur, wenn
// ueberhaupt schon einmal ein Spiel lief (_gLedsOwned), sonst wuerde schon der
// allererste loop()-Tick nach dem Boot (mode != GAME, nie gespielt) main.cpp
// dazu bringen, eventuell manuell gesetzte LEDs sofort wieder auszuschalten.
DisplayManager::GameLedState DisplayManager::gameLedState(uint8_t idx) const {
  if (_mode == GAME) {
    if (_gBombFlashLed == (int8_t)idx) return GLED_FLASH;
    bool used = (idx == 0) ? (_gBombs < 2) : (_gBombs < 1);
    return used ? GLED_USED : GLED_AVAILABLE;
  }
  return _gLedsOwned ? GLED_USED : GLED_NONE;
}

// Score sichern (+ Top-Score bei Bedarf aktualisieren/persistieren), Anzeige
// raeumen, Ergebnis als EIN durchlaufender Scrolltext ausgeben -- "GAME OVER",
// "SCORE n" und "TOP n" laufen dadurch abwechselnd ueber die Matrix, ohne
// eine eigene Umschalt-Logik/State-Machine noetig zu machen: die bestehende
// Endlos-Wiederholung von TEXT_SCROLL (siehe loop()/renderRotated())
// wiederholt einfach den ganzen String, "alternierend" ist hier also nur eine
// Frage der Textzusammensetzung, kein neuer Mechanismus. WICHTIG: der
// Leerraum zwischen den drei Phrasen muss mindestens eine Displaybreite
// (cols Spalten) betragen, sonst ist beim Scrollen zeitweise mehr als eine
// Phrase gleichzeitig sichtbar. textWidth(" ") liefert die tatsaechliche
// Pixelbreite eines Leerzeichens in der aktuell gesetzten (verschobenen)
// Schrift -- robuster als eine geratene Zeichenzahl.
void DisplayManager::gameOverEnd() {
  if (_gScore > _gTopScore) {
    _gTopScore = _gScore;
    File f = LittleFS.open(TOPSCORE_PATH, "w");
    if (f) { f.print(_gTopScore); f.close(); }
  }
  uint16_t score = _gScore;
  uint16_t top   = _gTopScore;
  clearHardware();

  uint16_t cols   = (uint16_t)_numDevices * 8;
  uint16_t spaceW = textWidth(" ");
  if (spaceW == 0) spaceW = 4;   // Sicherheitsnetz, falls die Schrift 0px liefert
  uint16_t gapChars = (uint16_t)(cols / spaceW) + 1;
  String gap; gap.reserve(gapChars);
  for (uint16_t i = 0; i < gapChars; i++) gap += ' ';

  // Immer normale Laufrichtung (Eintritt rechts), unabhaengig von der
  // persistierten Scrollrichtungs-Einstellung -- sonst liefe bei
  // scrollReverse=true die Reihenfolge sichtbar rueckwaerts (TOP zuerst statt
  // GAME OVER zuerst, da bei umgekehrter Laufrichtung das ENDE des Strings
  // zuerst ins Bild kommt). Nur fuer diesen einen Aufruf gesetzt, siehe
  // renderContent().
  _gForceLeftScroll = true;
  scrollMessage("GAME OVER" + gap + "SCORE " + String(score) + gap +
                "TOP " + String(top) + gap);
  _gForceLeftScroll = false;
}

// Startet das nicht-blockierende Start-Intro: "umgekehrter Pixel-Fill" --
// beginnt beim vollen (voll ausgeleuchteten) Bild, statt wie BOOT_FILL bei
// leer/aufbauend, und loest sich per Fisher-Yates-Mischung (dieselbe Technik,
// gleicher gemischter Puffer _animFillIdx wie BOOT_FILL -- Boot-Animation und
// Spiel laufen nie gleichzeitig) zufaellig auf, bis nichts mehr leuchtet.
// Danach uebernimmt startGameTitle() den Titel-Scroll.
void DisplayManager::startGameIntro() {
  uint16_t cols  = (uint16_t)_numDevices * 8;
  uint16_t total = cols * 8; if (total > sizeof(_animFillIdx)) total = (uint16_t)sizeof(_animFillIdx);
  for (uint16_t i = 0; i < total; i++) _animFillIdx[i] = (uint8_t)i;
  for (uint16_t i = (total > 0 ? total - 1 : 0); i > 0; i--) {
    uint16_t j = (uint16_t)random(0, i + 1);
    uint8_t tmp = _animFillIdx[i]; _animFillIdx[i] = _animFillIdx[j]; _animFillIdx[j] = tmp;
  }
  for (uint16_t c = 0; c < cols; c++) _animFillFrame[c] = 0xFF;   // voll
  _gIntroStep     = 0;
  _gIntroLastStep = millis();
  _gIntroActive   = true;
  _p.displayClear();
  for (uint16_t c = 0; c < cols; c++) writeLogicalColumn(c, _animFillFrame[c]);   // sofort voll zeichnen
}

// Zeichnet einen Schritt des Intros, gedrosselt per millis() (dieselbe
// Kadenz wie BOOT_FILL: alle 15ms 5 weitere Pixel). Loescht -- umgekehrt zu
// BOOT_FILL -- statt zu zuenden. Ist die gemischte Reihenfolge komplett
// abgearbeitet, folgt automatisch startGameTitle().
void DisplayManager::stepGameIntro() {
  unsigned long now = millis();
  if (now - _gIntroLastStep < 15) return;
  _gIntroLastStep = now;

  uint16_t cols  = (uint16_t)_numDevices * 8;
  uint16_t total = cols * 8; if (total > sizeof(_animFillIdx)) total = (uint16_t)sizeof(_animFillIdx);
  const uint16_t perStep = 5;
  uint16_t start = _gIntroStep * perStep;
  uint16_t end   = start + perStep; if (end > total) end = total;
  for (uint16_t i = start; i < end; i++) {
    uint8_t pos = _animFillIdx[i];
    uint8_t col = pos / 8, row = pos % 8;
    _animFillFrame[col] = (uint8_t)(_animFillFrame[col] & ~(1 << row));
  }
  for (uint16_t c = 0; c < cols; c++) writeLogicalColumn(c, _animFillFrame[c]);

  if (end >= total) { _gIntroActive = false; startGameTitle(); }
  else               _gIntroStep++;
}

// Einmaliger "PIXEL ATTACK"-Scroll nach dem Intro. Nutzt bewusst die private
// renderContent() statt der oeffentlichen scrollMessage() -- die wuerde
// _mode auf TEXT_SCROLL setzen und damit endgueltig aus GAME herausfuehren
// (wie beim Game-Over-Bildschirm gewollt); hier soll _mode dagegen GAME
// bleiben, damit renderGame() danach nahtlos ins eigentliche Gameplay
// uebergeht. Die Dauer wird aus der tatsaechlichen Textbreite berechnet
// (wie schon der Leerraum in gameOverEnd()), damit der Text einmal komplett
// durchlaeuft -- weder abgeschnitten noch mit toter Wartezeit am Ende.
void DisplayManager::startGameTitle() {
  static const char* TITLE = "PIXEL ATTACK";
  uint16_t cols = (uint16_t)_numDevices * 8;
  clearHardware();
  _p.displayClear();
  _gForceLeftScroll = true;   // immer Eintritt rechts, wie beim Game-Over-Text (siehe dortigen Kommentar)
  renderContent(TITLE, true);
  _gForceLeftScroll = false;
  uint16_t distance = textWidth(TITLE) + cols;   // Text muss komplett ein- und auslaufen
  _gTitleDurationMs = (unsigned long)distance * 50;   // 50ms/Spalte, wie renderContent()
  _gTitleStart  = millis();
  _gTitleActive = true;
}

// Spiel-Tick, gedrosselt per millis() wie stepBootAnimation() -- kein delay(),
// loop() bleibt fuer Web/MQTT/Seriell jederzeit ansprechbar.
void DisplayManager::renderGame() {
  if (_gIntroActive) { stepGameIntro(); return; }
  if (_gTitleActive) {
    // Treibt denselben Scroll an, den loop() ausserhalb von GAME uebernehmen
    // wuerde (siehe dortigen Kommentar) -- hier aber ohne _mode zu wechseln
    // und mit einer festen Dauer statt Scroll-Ende-Erkennung, da beide
    // Renderpfade (Parola / eigener Renderer) unterschiedliche Zykluslaengen
    // haben und fuer diesen kosmetischen Titel-Scroll keine Millisekunde
    // exakt stimmen muss.
    if (_orientation == 0) _p.displayAnimate();
    else                   renderRotated();
    if (millis() - _gTitleStart >= _gTitleDurationMs) {
      _gTitleActive = false;
      clearHardware();
      _p.displayClear();
      _gLastTick = _gLastSpawn = millis();   // Gameplay startet jetzt "frisch"
    }
    return;
  }

  static const unsigned long GAME_TICK_MS  = 220;   // etwas langsamer als zuvor (160)
  static const unsigned long GAME_SPAWN_MS = 900;

  unsigned long now = millis();
  if (now - _gLastTick < GAME_TICK_MS) return;
  _gLastTick = now;

  uint16_t cols = (uint16_t)_numDevices * 8;   // 32

  // Spawn: freien Slot links (Spalte 0) mit zufaelliger Zeile und Breite
  // (1-3 Spalten, 50/35/15%) belegen -- unterschiedlich "grosse" Invader.
  if (now - _gLastSpawn >= GAME_SPAWN_MS) {
    _gLastSpawn = now;
    for (uint8_t i = 0; i < GAME_MAX_INV; i++) {
      if (!_gInv[i].alive) {
        long r = random(0, 100);
        _gInv[i].alive = true;
        _gInv[i].x = 0;
        _gInv[i].y = (uint8_t)random(0, 8);
        _gInv[i].w  = (r < 55) ? 1 : (r < 85) ? 2 : 3;
        _gInv[i].hp = _gInv[i].w;   // muss so oft getroffen werden, wie er breit ist
        break;
      }
    }
  }

  // Bomben-Explosion: waehrend sie laeuft, "frieren" Invader ein (weder
  // Bewegung noch Verteidiger-Kollision) und blinken nur noch beim Zeichnen
  // (siehe unten) -- nach Ablauf sind alle weg. Zaehlt genauso wie normale
  // Treffer (ein Punkt je getroffenem Invader): nur 2 Bomben pro Runde
  // verfuegbar, das begrenzt den Nutzen von selbst -- kein Grund, sie
  // schlechter zu bepunkten als gezielte Schuesse.
  bool bombFlashing = (_gBombFlashSteps > 0);
  if (bombFlashing) {
    _gBombFlashSteps--;
    if (_gBombFlashSteps == 0) {
      for (uint8_t i = 0; i < GAME_MAX_INV; i++) {
        if (_gInv[i].alive) _gScore++;
        _gInv[i].alive = false;
      }
      _gBombFlashLed = -1;   // Signal an main.cpp: Explosion vorbei, LED aus
    }
  } else {
    // Invader bewegen; erreicht die Vorderkante (x+w-1) die Verteidiger-Spalte,
    // entscheidet die Zeile ueber Treffer (Ausweich-Mechanik: rauf/runter
    // schuetzt) oder folgenloses Entkommen auf einer anderen Zeile.
    for (uint8_t i = 0; i < GAME_MAX_INV; i++) {
      if (!_gInv[i].alive) continue;
      _gInv[i].x++;
      int16_t front = _gInv[i].x + _gInv[i].w - 1;
      if (front >= (int16_t)cols - 1) {
        if (_gInv[i].y == _gDefY && _gLives > 0) _gLives--;
        _gInv[i].alive = false;
      }
    }
  }

  // Schuesse bewegen + Kollision. WICHTIG: Schuss (x--) und Invader-Vorderkante
  // (x++) aendern ihren Abstand pro Tick um 2 -- bei ungeradem Startabstand
  // wuerden sie bei reiner Gleichheitspruefung exakt aneinander vorbeispringen,
  // ohne je gleichauf zu sein (Bsp.: Schuss 5/Kante 4 -> naechster Tick Schuss
  // 4/Kante 5, niemals x==x). Deshalb als Treffer werten, sobald der Schuss die
  // Vorderkante in diesem Tick erreicht ODER ueberquert hat.
  for (uint8_t b = 0; b < GAME_MAX_BULLETS; b++) {
    if (!_gBullet[b].alive) continue;
    _gBullet[b].x--;
    if (_gBullet[b].x < 0) {
      _gBullet[b].alive = false;   // ins Leere geschossen
      continue;
    }
    for (uint8_t i = 0; i < GAME_MAX_INV; i++) {
      int16_t front = _gInv[i].x + _gInv[i].w - 1;
      if (_gInv[i].alive && _gInv[i].y == _gBullet[b].y && _gBullet[b].x <= front) {
        if (_gInv[i].hp > 0) _gInv[i].hp--;
        if (_gInv[i].hp == 0) _gInv[i].alive = false;   // erst bei hp==0 wirklich zerstoert
        _gBullet[b].alive = false;                      // Schuss ist so oder so verbraucht
        _gScore++;                                      // ein Punkt je Treffer, nicht nur je Kill
        break;
      }
    }
  }

  if (_gLives == 0) { gameOverEnd(); return; }

  uint8_t colBuf[32] = {0};
  colBuf[cols - 1] |= (uint8_t)(1 << _gDefY);
  for (uint8_t b = 0; b < GAME_MAX_BULLETS; b++) {
    if (_gBullet[b].alive) colBuf[_gBullet[b].x] |= (uint8_t)(1 << _gBullet[b].y);
  }
  // Waehrend der Bomben-Explosion nur in jedem zweiten Tick zeichnen -> Blinken.
  bool showInvaders = !bombFlashing || (_gBombFlashSteps % 2 == 0);
  if (showInvaders) {
    for (uint8_t i = 0; i < GAME_MAX_INV; i++) {
      if (!_gInv[i].alive) continue;
      for (uint8_t k = 0; k < _gInv[i].w; k++) colBuf[_gInv[i].x + k] |= (uint8_t)(1 << _gInv[i].y);
    }
  }
  for (uint16_t c = 0; c < cols; c++) writeLogicalColumn(c, colBuf[c]);
}

void DisplayManager::setBrightness(uint8_t level) {
  if (level > 15) level = 15;
  bool changed = (level != _brightness);
  _brightness = level;
  _p.setIntensity(level);
  if (changed) {                               // nur bei Aenderung speichern (Flash schonen)
    File f = LittleFS.open(BRIGHT_PATH, "w");
    if (f) { f.print(level); f.close(); }
  }
}

void DisplayManager::setBrightnessOverride(uint8_t level) {
  if (level > 15) level = 15;
  _p.setIntensity(level);                  // wirkt sofort, _brightness (gespeicherter Wert) bleibt unangetastet
}

void DisplayManager::clearBrightnessOverride() {
  _p.setIntensity(_brightness);
}

// Aendert die Ausrichtung (0/180 -- 90/270 wieder entfernt, siehe Klassen-
// kommentar in DisplayManager.h), persistiert sie und baut den aktuell
// sichtbaren Inhalt fuer die neue Ausrichtung sofort neu auf.
void DisplayManager::setOrientation(uint16_t degrees) {
  if (degrees != 180) degrees = 0;
  bool changed = (degrees != _orientation);
  if (!changed) return;
  _orientation = degrees;

  File f = LittleFS.open(ORIENT_PATH, "w");
  if (f) { f.print(degrees); f.close(); }

  _rotPos = 0;
  if (_orientation == 0) {
    _p.displayClear();
    if (_mode == TEXT_STATIC || _mode == TEXT_SCROLL) renderContent(_renderText, _mode == TEXT_SCROLL);
    else if (_mode == TIMER) { _lastShown = -1; renderTimer(); }
    else if (_mode == CLOCK) { _lastShown = -1; renderClock(); }
  } else {
    if (_mode == TEXT_STATIC || _mode == TEXT_SCROLL) renderContent(_renderText, _mode == TEXT_SCROLL);
    else if (_mode == TIMER) { _lastShown = -1; renderTimer(); }
    else if (_mode == CLOCK) { _lastShown = -1; renderClock(); }
    else _rotDirty = true;   // IDLE -> naechster renderRotated() loescht die Hardware sauber
  }
}

// Aendert die Scrollrichtung, persistiert sie und baut den aktuell sichtbaren
// Inhalt (falls gerade scrollend) sofort mit der neuen Richtung neu auf.
void DisplayManager::setScrollReverse(bool reverse) {
  if (reverse == _scrollReverse) return;
  _scrollReverse = reverse;

  File f = LittleFS.open(SCROLLDIR_PATH, "w");
  if (f) { f.print(reverse ? "1" : "0"); f.close(); }

  if (_mode == TEXT_SCROLL) { _rotPos = 0; renderContent(_renderText, true); }
}

// Schreibt eine "logische" (unrotierte) Spalte an die Hardware -- dieselbe
// Orientierungs-Transformation wie renderRotated() (siehe dessen Kommentar
// zur Spalten-/Zeilen-Herleitung): volle Spalten (0xFF) sehen in beiden
// Orientierungen gleich aus, Bit 7 (0x80) ist in beiden Faellen die visuell
// UNTERSTE Zeile (gleiche Konvention wie die Schrift, siehe buildShiftedFont()).
// Aktualisiert bei orientation==180 zusaetzlich den _lastCols-Cache, aus dem
// frameJson() dort liest (bei orientation==0 liest frameJson() direkt aus dem
// MD_MAX72XX-Puffer, den setColumn() ohnehin aktualisiert).
void DisplayManager::writeLogicalColumn(uint16_t col, uint8_t logicalByte) {
  uint8_t v = (_orientation == 0) ? logicalByte : reverseBits8(logicalByte);
  _p.getGraphicObject()->setColumn(col, v);
  if (_orientation != 0 && col < sizeof(_lastCols)) _lastCols[col] = v;
}

void DisplayManager::clearHardware() {
  uint16_t cols = (uint16_t)_numDevices * 8;
  for (uint16_t c = 0; c < cols; c++) writeLogicalColumn(c, 0);
}

// Startet eine Boot-Animation NICHT-blockierend: setzt nur den Zustand
// (inkl. Vorbereitung je Typ), das eigentliche Zeichnen uebernimmt
// stepBootAnimation() ueber loop(). type==BOOT_OFF ergibt einen inaktiven
// Zustand (animationActive()==false), also ein No-Op.
void DisplayManager::startBootAnimation(uint8_t type) {
  _animType = type;
  _animStep = 0;
  _animLastStep = millis();
  clearHardware();
  if (type == BOOT_FILL) {
    // 256 (Spalte,Zeile)-Positionen zufaellig mischen (Fisher-Yates) --
    // garantiert vollstaendige Abdeckung in fester Zeit, anders als rein
    // zufaelliges Ziehen mit moeglichen Wiederholungen.
    uint16_t cols = (uint16_t)_numDevices * 8;
    uint16_t total = cols * 8;
    if (total > 256) total = 256;
    for (uint16_t i = 0; i < total; i++) _animFillIdx[i] = (uint8_t)i;
    for (uint16_t i = (total > 0 ? total - 1 : 0); i > 0; i--) {
      uint16_t j = (uint16_t)random(0, i + 1);
      uint8_t tmp = _animFillIdx[i]; _animFillIdx[i] = _animFillIdx[j]; _animFillIdx[j] = tmp;
    }
    for (uint16_t c = 0; c < sizeof(_animFillFrame); c++) _animFillFrame[c] = 0;
  } else if (type == BOOT_PROGRESS) {
    _bootProgressCols = 0;   // Vorschau simuliert den Balken fest getaktet, siehe stepBootAnimation()
  }
  _animActive = (type != BOOT_OFF);
}

// Zeichnet einen Frame der aktiven Boot-Animation, gedrosselt per millis()
// (kein delay() -> loop() bleibt fuer alle anderen Aufgaben, insbesondere
// den Webserver, jederzeit ansprechbar). Setzt _animActive=false, sobald die
// Animation fertig ist.
void DisplayManager::stepBootAnimation() {
  unsigned long now = millis();
  uint16_t cols = (uint16_t)_numDevices * 8;

  switch (_animType) {
    case BOOT_SCAN: {   // 3 Spalten breiter Balken laeuft einmal durch, kein Auffuellen
      if (now - _animLastStep < 18) return;
      _animLastStep = now;
      if (_animStep > cols + 2) { clearHardware(); _animActive = false; return; }
      for (uint16_t c = 0; c < cols; c++) {
        int32_t d = (int32_t)_animStep - (int32_t)c;
        writeLogicalColumn(c, (d >= 0 && d <= 2) ? 0xFF : 0x00);
      }
      _animStep++;
      break;
    }
    case BOOT_FILL: {   // Gemischte Reihenfolge gruppenweise anzuenden, dann kurz halten
      if (_animStep == 0xFFFF) {
        if (now - _animLastStep >= 150) { clearHardware(); _animActive = false; }
        return;
      }
      if (now - _animLastStep < 15) return;
      _animLastStep = now;
      uint16_t total = cols * 8; if (total > 256) total = 256;
      const uint16_t perStep = 5;
      uint16_t start = _animStep * perStep;
      uint16_t end   = start + perStep; if (end > total) end = total;
      for (uint16_t i = start; i < end; i++) {
        uint8_t pos = _animFillIdx[i];
        uint8_t col = pos / 8, row = pos % 8;
        _animFillFrame[col] = (uint8_t)(_animFillFrame[col] | (1 << row));
      }
      for (uint16_t c = 0; c < cols; c++) writeLogicalColumn(c, _animFillFrame[c]);
      if (end >= total) { _animStep = 0xFFFF; _animLastStep = now; }
      else _animStep++;
      break;
    }
    case BOOT_PROGRESS: {   // NUR die Vorschau (simulierter fester Durchlauf, ~1.2s);
                             // der echte Boot-Balken laeuft ueber bootProgressBegin()/-Update(),
                             // siehe main.cpp -- diese Zustandsmaschine ist dort nicht aktiv.
      const unsigned long durationMs = 1200;
      unsigned long elapsed = now - _animLastStep;
      if (elapsed >= durationMs + 150) { clearHardware(); _animActive = false; return; }
      float frac = (float)elapsed / (float)durationMs;
      bootProgressUpdate(frac > 1.0f ? 1.0f : frac);
      break;
    }
    default: _animActive = false; break;   // BOOT_OFF
  }
}

// Aendert die Einschalt-Animation, persistiert sie (nur bei Aenderung, Flash
// schonen) und startet sie danach IMMER einmal -- Sofort-Vorschau beim
// Auswaehlen wie orient()/scrollDir(), gleichzeitig die Grundlage fuer den
// "Vorschau"-Button in den Einstellungen (ruft dieselbe Aktion mit dem
// aktuell gewaehlten Wert erneut auf). Nicht-blockierend: kehrt sofort
// zurueck, loop()/stepBootAnimation() spielt die Animation im Hintergrund ab.
void DisplayManager::setBootAnimation(uint8_t type) {
  if (type > 3) type = BOOT_SCAN;
  if (type != _bootAnim) {
    _bootAnim = type;
    File f = LittleFS.open(BOOTANIM_PATH, "w");
    if (f) { f.print(type); f.close(); }
  }
  startBootAnimation(type);
}

// Bereitet den (an den echten WiFi-Connect gekoppelten) Boot-Fortschrittsbalken
// vor. main.cpp ruft bootProgressUpdate() waehrend der Wartezeit wiederholt auf.
void DisplayManager::bootProgressBegin() {
  clearHardware();
  _bootProgressCols = 0;
}

// frac01: 0..1 (Anteil der bereits vergangenen Wartezeit). Zeichnet nur bei
// Aenderung neu (spart SPI-Schreibzugriffe). Balken sitzt in der visuell
// untersten Zeile (Bit 7, siehe writeLogicalColumn()-Kommentar).
void DisplayManager::bootProgressUpdate(float frac01) {
  if (frac01 < 0.0f) frac01 = 0.0f;
  if (frac01 > 1.0f) frac01 = 1.0f;
  uint16_t cols = (uint16_t)_numDevices * 8;
  uint16_t target = (uint16_t)(frac01 * cols + 0.5f);
  if (target == _bootProgressCols) return;
  for (uint16_t c = _bootProgressCols; c < target; c++) writeLogicalColumn(c, 0x80);
  _bootProgressCols = target;
}

void DisplayManager::renderTimer() {
  if (_timerPaused) return;           // Anzeige bleibt eingefroren, bis resumeTimer() laeuft
  long elapsed = (long)((millis() - _timerStart) / 1000UL);
  long value   = _countdown ? (_timerBase - elapsed) : elapsed;
  if (value < 0) value = 0;
  if (value == _lastShown) return;          // nur bei Sekundenwechsel neu zeichnen
  _lastShown = value;
  renderContent(formatTime(value), false);
}

void DisplayManager::renderClock() {
  // Anzeige braucht nur Minutenaufloesung -> Zeit nur ~2x/s pruefen statt in
  // jeder loop()-Iteration (spart tausende localtime()-Aufrufe pro Sekunde).
  if (millis() - _lastClockCheck < 500) return;
  _lastClockCheck = millis();
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  if (t == nullptr) return;
  long minuteOfDay = t->tm_hour * 60 + t->tm_min;
  if (minuteOfDay == _lastShown) return;     // nur bei Minutenwechsel neu zeichnen
  _lastShown = minuteOfDay;
  char b[8];
  snprintf(b, sizeof(b), "%02d:%02d", t->tm_hour, t->tm_min);
  renderContent(String(b), false);
}

// Gemeinsamer Ausgabepfad: bei orientation==0 wie bisher direkt ueber Parola
// (Scroll-Engine unveraendert); sonst ueber den eigenen Renderer (siehe
// buildRotBuf()/renderRotated() und die Erklaerung in DisplayManager.h).
// _buf wird immer gefuellt, da stateJson() im Uhr-Modus daraus liest.
void DisplayManager::renderContent(const String& text, bool scroll) {
  text.toCharArray(_buf, sizeof(_buf));
  // _gForceLeftScroll (nur waehrend gameOverEnd()/startGameTitle() gesetzt)
  // erzwingt normale Laufrichtung (Eintritt rechts) unabhaengig von der
  // persistierten Nutzereinstellung -- einmal hier "eingebrannt", da weder
  // Parolas Effekt-Parameter noch _rotReverse (siehe renderRotated()) danach
  // pro Frame neu ausgewertet werden, sondern nur bei jedem neuen Scroll-Start.
  bool effReverse = _scrollReverse && !_gForceLeftScroll;
  if (_orientation == 0) {
    if (scroll) {
      textEffect_t eff = effReverse ? PA_SCROLL_RIGHT : PA_SCROLL_LEFT;
      _p.displayText(_buf, PA_LEFT, 50, 0, eff, eff);
    }
    else        _p.displayText(_buf, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
    _p.displayReset();
  } else {
    _rotReverse = effReverse;
    buildRotBuf(text);
  }
}

// Reiht die Font-Spalten aller Zeichen aneinander (je 1 Leerspalte Abstand,
// wie textWidth()) -- die Grundlage fuer den eigenen rotierten Renderer.
void DisplayManager::buildRotBuf(const String& text) {
  MD_MAX72XX* g = _p.getGraphicObject();
  uint8_t cbuf[16];
  _rotLen = 0;
  for (uint16_t i = 0; i < text.length() && _rotLen < sizeof(_rotBuf) - 1; i++) {
    uint8_t w = g->getChar((uint8_t)text[i], sizeof(cbuf), cbuf);
    for (uint8_t k = 0; k < w && _rotLen < sizeof(_rotBuf); k++) _rotBuf[_rotLen++] = cbuf[k];
    if (_rotLen < sizeof(_rotBuf)) _rotBuf[_rotLen++] = 0;   // 1 Spalte Zeichenabstand
  }
  _rotPos   = 0;
  _rotDirty = true;
}

// Zeichnet ein Frame fuer orientation == 180: baut das aktuell sichtbare
// (unrotierte) 32x8-Fenster aus _rotBuf (zentriert, wenn es passt, sonst ab
// _rotPos scrollend), dreht es um 180 Grad und schreibt es direkt via
// setColumn(). Laeuft an Parolas Scroll-Engine vorbei (siehe Klassenkommentar
// in DisplayManager.h) -- Parola/MD_MAX72XX-Puffer bleiben dabei unberuehrt.
//
// 180 Grad: out[c] = reverseBits8(win[c]) -- NUR die Zeilen-Bits je Spalte
// spiegeln, OHNE die Spaltenreihenfolge zu tauschen. Unintuitiv, aber am
// Geraet verifiziert (August 2026): getColumn(0)/setColumn(0) ist bei
// diesem Board/FC16_HW die RECHTE Seite (siehe CLAUDE.md, die Web-Vorschau
// dreht die Spalten deshalb extra um) -- die Hardware spiegelt die
// Spaltenreihenfolge also bereits selbst. Ein zusaetzliches Spiegeln der
// Spaltenreihenfolge im Code (wie urspruenglich hier) hebt sich mit diesem
// Hardware-Effekt zu einer reinen Spiegelung auf, statt einer sauberen
// 180-Grad-Drehung -- das war ein am Geraet beobachteter Bug
// (mit Spaltentausch: spiegelverkehrt; nur Zeilen-Bits: korrekt gedreht).
//
// 90/270 Grad waren zwischenzeitlich ebenfalls implementiert, lieferten aber
// kein von 0 Grad unterscheidbares Bild -- optisch nicht ueberzeugend und
// daher (August 2026) wieder entfernt. Nur 0/180 werden unterstuetzt.
void DisplayManager::renderRotated() {
  uint16_t cols = (uint16_t)_numDevices * 8;
  if (cols == 0 || cols > sizeof(_lastCols)) return;

  // Scroll-Zyklus (nur wenn scroll==true): [cols Spalten leerer Vorlauf] +
  // [_rotLen Spalten Text] + [cols Spalten leerer Nachlauf], danach von vorn.
  // Der Vorlauf sorgt dafuer, dass der Text von der Kante hereinwandert statt
  // sofort das ganze Fenster zu fuellen (wie Parolas PA_SCROLL_LEFT/RIGHT).
  bool scroll = _rotLen > cols;
  uint16_t cycle = (uint16_t)(_rotLen + 2 * cols);
  if (scroll) {
    if (millis() - _rotLastStep >= 50) {
      _rotLastStep = millis();
      if (_rotReverse) {
        if (_rotPos == 0) _rotPos = (uint16_t)(cycle - 1);   // von vorn, am Ende weiter
        else              _rotPos--;
      } else {
        _rotPos++;
        if (_rotPos >= cycle) _rotPos = 0;
      }
      _rotDirty = true;
    }
  }
  if (!_rotDirty) return;
  _rotDirty = false;

  uint8_t win[64];
  uint16_t pad = (!scroll && cols > _rotLen) ? (uint16_t)((cols - _rotLen) / 2) : 0;
  for (uint16_t c = 0; c < cols; c++) {
    long idx = scroll ? (long)_rotPos + c - (long)cols : (long)c - (long)pad;
    win[c] = (idx >= 0 && idx < (long)_rotLen) ? _rotBuf[idx] : 0;
  }

  // Nur Zeilen-Bits je Spalte spiegeln, OHNE die Spaltenreihenfolge zu tauschen:
  // getColumn(0)/setColumn(0) ist bei diesem Board/FC16_HW die RECHTE Seite
  // (siehe CLAUDE.md, die Web-Vorschau dreht die Spalten deswegen extra um) --
  // die Hardware spiegelt die Spaltenreihenfolge also bereits selbst. Ein
  // zusaetzliches Spiegeln hier wuerde sich mit diesem Hardware-Effekt zu einer
  // reinen Spiegelung (statt sauberer 180-Grad-Drehung) aufheben -- genau das
  // Symptom, das am Geraet beobachtet wurde.
  MD_MAX72XX* g = _p.getGraphicObject();
  for (uint16_t c = 0; c < cols; c++) {
    uint8_t v = reverseBits8(win[c]);   // nur orientation==180 erreicht renderRotated()
    _lastCols[c] = v;
    g->setColumn(c, v);
  }
}

// Restsekunden bis zum Ablauf des Countdowns (0 sobald abgelaufen), sonst -1.
// Basis der automatischen LED-Warnung (siehe LedController). Nur echter
// Countdown-Modus zaehlt; Hochzaehlen und andere Modi liefern -1.
long DisplayManager::timerRemaining() const {
  if (_mode != TIMER || !_countdown) return -1;
  if (_timerPaused) return _pausedRemaining;   // eingefroren, keine Live-Berechnung
  long elapsed = (long)((millis() - _timerStart) / 1000UL);
  long value   = _timerBase - elapsed;
  return value < 0 ? 0 : value;
}

String DisplayManager::formatTime(long secs) {
  long h = secs / 3600, m = (secs % 3600) / 60, s = secs % 60;
  char b[16];
  if (h > 0) snprintf(b, sizeof(b), "%ld:%02ld:%02ld", h, m, s);
  else       snprintf(b, sizeof(b), "%02ld:%02ld", m, s);
  return String(b);
}

String DisplayManager::stateJson() const {
  String mode, body;
  switch (_mode) {
    case TIMER: mode = "timer"; body = formatTime(_lastShown); break;
    case CLOCK: mode = "clock"; body = _buf;                   break;
    case IDLE:  mode = "idle";  body = "";                     break;
    case GAME:  mode = "game";  body = "";                     break;
    default:    mode = "text";  body = _text;                  break;
  }
  return "{\"mode\":\"" + mode + "\",\"text\":\"" + body +
         "\",\"brightness\":" + String(_brightness) +
         ",\"orientation\":" + String(_orientation) +
         ",\"scrollReverse\":" + (_scrollReverse ? "true" : "false") +
         ",\"bootAnim\":" + String(_bootAnim) +
         ",\"timerPaused\":" + (_timerPaused ? "true" : "false") +
         ",\"gameScore\":" + String(_gScore) +
         ",\"gameLives\":" + String(_gLives) +
         ",\"gameBombs\":" + String(_gBombs) +
         ",\"gameTop\":" + String(_gTopScore) + "}";
}

// Liest den aktuellen Pixelpuffer der Kette aus: ein Byte je Spalte (Bit r = Zeile r).
// Spiegelt live auch die Scroll-Animation, da MD_MAX72XX fortlaufend gepuffert wird.
// Bei orientation != 0 kommt der Puffer aus dem eigenen Renderer (_lastCols),
// da der eigentliche MD_MAX72XX-Puffer dann unrotiert/ungenutzt bleibt.
String DisplayManager::frameJson() {
  uint16_t cols = (uint16_t)_numDevices * 8;   // 4 Module * 8 = 32 Spalten
  MD_MAX72XX* g = (_orientation == 0) ? _p.getGraphicObject() : nullptr;
  String out;
  out.reserve(cols * 4 + 2);
  out = "[";
  for (uint16_t c = 0; c < cols; c++) {
    if (c) out += ',';
    out += String(g ? g->getColumn(c) : _lastCols[c]);
  }
  out += "]";
  return out;
}
