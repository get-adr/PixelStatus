#pragma once
#include <Arduino.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>

// Kapselt die MAX7219-Kette und kennt drei Modi: Leerlauf, Text (statisch
// oder scrollend) und Timer (Countdown / Hochzaehlen). loop() muss in jedem
// Arduino-loop() aufgerufen werden, damit Animation und Timer laufen.
//
// Ausrichtung (0/180, siehe setOrientation()): Bei 0 Grad rendert wie bisher
// Parola direkt (Scroll-Engine unveraendert, volle Effizienz/Kompatibilitaet).
// Bei 180 Grad wird Parolas Scroll-Engine NICHT genutzt -- sie verschiebt den
// MD_MAX72XX-Puffer inkrementell (transform(TSL)), was beim Ueberschreiben mit
// gedrehten Daten kollidieren wuerde. Stattdessen baut ein eigener, kleiner
// Renderer den Text direkt aus den Font-Glyphen (ueber getChar(), dieselbe API wie
// textWidth()) in einen eigenen Spaltenpuffer, schiebt den bei Bedarf selbst
// weiter und schreibt pro Frame das um 180 Grad gedrehte 32x8-Fenster direkt
// ueber setColumn(). Siehe DisplayManager.cpp fuer die Herleitung der Formel
// (90/270 Grad waren zwischenzeitlich ebenfalls implementiert, brachten aber
// kein von 0 Grad unterscheidbares Bild und wurden wieder entfernt).
class DisplayManager {
public:
  DisplayManager(uint8_t csPin, uint8_t numDevices);

  void begin(uint8_t brightness);
  void loop();

  void showMessage(const String& text);   // scrollt automatisch, wenn zu lang
  void scrollMessage(const String& text);  // immer scrollend
  void startTimer(long seconds, bool countdown);
  void pauseTimer();                       // friert die Anzeige und die Restzeit-Berechnung ein
  void resumeTimer();                      // laeuft ab der eingefrorenen Restzeit weiter
  bool timerPaused() const { return _timerPaused; }
  void stopTimer();
  void startClock();                       // zeigt HH:MM aus der Systemzeit (NTP/USB)
  void clear();
  void setBrightness(uint8_t level);       // 0..15, persistiert in LittleFS
  // Temporaere Helligkeit, OHNE die gespeicherte Einstellung zu ueberschreiben
  // (z.B. Akku-Warnung dimmt runter, ohne den Nutzerwert zu verlieren).
  void setBrightnessOverride(uint8_t level);
  void clearBrightnessOverride();          // zurueck zur gespeicherten Helligkeit

  // Ausrichtung der Schrift: 0/180 Grad, persistiert in LittleFS, wirkt
  // sofort (kein Neustart). Ungueltige Werte werden auf 0 abgebildet.
  void     setOrientation(uint16_t degrees);
  uint16_t orientation() const { return _orientation; }

  // Scrollrichtung: false = Standard (links), true = umgekehrt (rechts).
  // Persistiert in LittleFS, wirkt sofort auf den aktuellen Inhalt.
  void setScrollReverse(bool reverse);
  bool scrollReverse() const { return _scrollReverse; }

  // Einschalt-Animation: fuellt die dunkle Luecke zwischen Boot und dem
  // ersten "WiFi..."-Scrolltext (siehe main.cpp). Persistiert in LittleFS
  // (/bootanim.txt), wirkt bei Auswahl in den Einstellungen sofort als
  // Vorschau (setBootAnimation() spielt immer ab, persistiert aber nur bei
  // Aenderung). NICHT blockierend: startBootAnimation() setzt nur den
  // Zustand, loop() treibt sie ueber stepBootAnimation() schrittweise an
  // (dasselbe Prinzip wie renderRotated()) -- ein HTTP-Request (Vorschau)
  // kehrt dadurch sofort zurueck, waehrend die Animation im Hintergrund
  // weiterlaeuft. Fuer den ECHTEN Boot (main.cpp) wartet ein einfaches
  // display.loop()-Schleifchen auf animationActive()==false, da dort ohnehin
  // noch nichts anderes laufen muss. BOOT_PROGRESS ist ein Sonderfall: beim
  // echten Boot an den echten WiFi-Connect gekoppelt (main.cpp treibt
  // bootProgressBegin()/bootProgressUpdate() direkt an, OHNE die Animations-
  // Zustandsmaschine); die Vorschau in den Einstellungen simuliert ihn
  // stattdessen ueber stepBootAnimation() fest getaktet (~1.2s).
  // (Urspruenglich gab es zusaetzlich BOOT_FADE/BOOT_LOGO -- optisch nicht
  // ueberzeugend und daher wieder entfernt.)
  enum BootAnim { BOOT_OFF = 0, BOOT_SCAN = 1, BOOT_FILL = 2, BOOT_PROGRESS = 3 };
  void    setBootAnimation(uint8_t type);
  uint8_t bootAnimation() const { return _bootAnim; }
  void    startBootAnimation(uint8_t type);   // nicht-blockierend; No-Op-Zustand bei BOOT_OFF
  bool    animationActive() const { return _animActive; }
  void    bootProgressBegin();               // nur fuer den echten Boot-Fortschrittsbalken
  void    bootProgressUpdate(float frac01);  // 0..1, main.cpp treibt ihn waehrend WiFi-Connect an

  String stateJson() const;                // fuer /api/state und MQTT-State
  String frameJson();                      // aktueller Pixelpuffer (32 Spalten) fuer die Web-Vorschau
  long   timerRemaining() const;           // Restsekunden im Countdown, sonst -1 (fuer die LED-Warnung)

  // Space-Invaders-Gimmick: reine Web-UI-Steuerung (Buttons/Pfeiltasten im
  // Browser), kein MQTT/USB-Anwendungsfall dafuer vorgesehen. Siehe
  // renderGame() in DisplayManager.cpp fuer Spiellogik/Kollisionstest.
  void gameStart();                        // startet neu -- auch als "Reset" waehrend/nach einem Spiel nutzbar
  void gameUp();
  void gameDown();
  void gameFire();
  void gameBomb();                         // sprengt alle sichtbaren Invader, verbraucht eine der 2 Bomben
  // main.cpp liest dies in jedem loop() ab (wie timerRemaining() fuer die LED-
  // Countdown-Warnung) und blinkt/loescht die zugehoerige Zusatz-LED darueber --
  // DisplayManager kennt LedController bewusst nicht (siehe Klassentrennung),
  // dieselbe Entkopplung wie bei der Timer-Warnung. Bewusst nur bei
  // tatsaechlichem Wechsel anwenden (siehe main.cpp/handleGameBombLed()) --
  // LedController::set() setzt bei jedem Aufruf mit s != BLINK auch das
  // globale Wechselblinken zurueck, staendiges Aufrufen wuerde das kaputt machen.
  enum GameLedState { GLED_NONE, GLED_AVAILABLE, GLED_FLASH, GLED_USED };
  GameLedState gameLedState(uint8_t idx) const;   // idx 0/1; GLED_NONE = Spiel nicht aktiv, LED unangetastet lassen

private:
  void setText(const String& text, bool scroll);
  uint16_t textWidth(const String& text);   // tatsaechliche Pixelbreite laut Font
  void renderTimer();
  void renderClock();
  void renderGame();                        // Spiel-Tick, gedrosselt per millis() wie stepBootAnimation()
  void gameOverEnd();                       // Score sichern, Anzeige raeumen, GAME OVER scrollen
  void startGameIntro();                    // "umgekehrter Pixel-Fill": volles Bild loest sich zufaellig auf
  void stepGameIntro();                     // von renderGame() gerufen, solange _gIntroActive
  void startGameTitle();                    // einmaliger "PIXEL ATTACK"-Scroll nach dem Intro, vor dem Gameplay
  static String formatTime(long secs);

  // Gemeinsamer Ausgabepfad fuer setText()/renderTimer()/renderClock(): bei
  // orientation==0 wie bisher ueber Parola, sonst ueber den eigenen Renderer.
  void renderContent(const String& text, bool scroll);
  void buildRotBuf(const String& text);    // eigener Renderer: Font-Spalten aneinanderreihen
  void renderRotated();                    // eigener Renderer: pro loop() ggf. weiterschieben + zeichnen

  // Boot-Animationen: schreiben direkt via setColumn() (wie renderRotated()),
  // nicht ueber Parola/Font. writeLogicalColumn() kapselt die Orientierungs-
  // Transformation (siehe renderRotated()-Kommentar) und den _lastCols-Cache.
  void writeLogicalColumn(uint16_t col, uint8_t logicalByte);
  void clearHardware();                    // alle sichtbaren Spalten loeschen (Animationsende)
  void stepBootAnimation();                // von loop() aufgerufen, solange _animActive

  MD_Parola _p;
  uint8_t   _numDevices;
  uint8_t   _brightness = 4;
  uint16_t  _orientation = 0;              // 0/180, siehe setOrientation()
  bool      _scrollReverse = false;        // false=links, true=rechts, siehe setScrollReverse()
  uint8_t   _bootAnim = BOOT_SCAN;         // siehe setBootAnimation()
  uint16_t  _bootProgressCols = 0;         // zuletzt gezeichnete Balkenbreite (bootProgressUpdate())

  // Zustand der nicht-blockierenden Boot-Animation (siehe stepBootAnimation()).
  bool          _animActive = false;
  uint8_t       _animType   = BOOT_OFF;
  uint16_t      _animStep   = 0;           // Bedeutung je Typ unterschiedlich (siehe .cpp)
  unsigned long _animLastStep = 0;         // Zeitpunkt des letzten Frames bzw. Startzeit (BOOT_PROGRESS)
  uint8_t       _animFillIdx[256];         // gemischte Pixel-Reihenfolge fuer BOOT_FILL
  uint8_t       _animFillFrame[32] = {0};  // bereits angezuendete Spalten fuer BOOT_FILL

  enum Mode { IDLE, TEXT_STATIC, TEXT_SCROLL, TIMER, CLOCK, GAME };
  Mode   _mode = IDLE;
  String _text;                            // Original (UTF-8) -- fuer stateJson()/Rundreise zur API
  String _renderText;                      // UTF-8->Latin-1 gewandelt -- tatsaechlich gerenderter Text
  char   _buf[80];                         // Parola scrollt ueber diesen Puffer -> muss leben

  bool          _countdown   = false;
  long          _timerBase   = 0;
  unsigned long _timerStart  = 0;
  bool          _timerPaused = false;
  long          _pausedRemaining = 0;      // Restsekunden, eingefroren bei pauseTimer() (nur Countdown)
  unsigned long _pauseStart  = 0;          // millis() bei pauseTimer(), verschiebt _timerStart bei resumeTimer()
  long          _lastShown   = -1;
  unsigned long _lastClockCheck = 0;       // drosselt die Zeitabfrage im Uhr-Modus

  // Eigener Renderer (nur bei orientation != 0 aktiv, siehe oben).
  uint8_t       _rotBuf[256];              // voller (unrotierter) Spaltenpuffer des aktuellen Textes
  uint16_t      _rotLen   = 0;             // genutzte Breite in _rotBuf (0 = nichts anzuzeigen)
  uint16_t      _rotPos   = 0;             // aktuelle Scroll-Startspalte (nur relevant, wenn _rotLen > Anzeigebreite)
  bool          _rotDirty = false;         // true -> naechster renderRotated()-Aufruf muss zeichnen
  unsigned long _rotLastStep = 0;
  uint8_t       _lastCols[64] = {0};       // zuletzt an die Hardware geschriebenes (rotiertes) Bild, fuer frameJson()
  // Laufrichtung, wie sie beim letzten renderContent()-Aufruf "eingebrannt"
  // wurde (renderRotated() liest pro Frame diese Kopie statt live _scrollReverse,
  // damit _gForceLeftScroll nur den EINEN Scroll-Start beeinflusst, siehe dort).
  bool          _rotReverse = false;

  // Nur waehrend gameOverEnd()/startGameTitle() kurz auf true gesetzt --
  // erzwingt fuer genau diesen einen Scroll-Start normale Laufrichtung
  // (Eintritt rechts), unabhaengig von der persistierten _scrollReverse-
  // Einstellung. Siehe renderContent().
  bool _gForceLeftScroll = false;

  // Space-Invaders-Gimmick (siehe renderGame() in der .cpp). Verteidiger steht
  // fest an der rechten Spalte und bewegt sich nur in der Zeile (_gDefY);
  // Angreifer spawnen links (Spalte 0, zufaellige Breite 1-3) und laufen nach
  // rechts. Bis zu GAME_MAX_BULLETS Schuesse gleichzeitig unterwegs. hp startet
  // bei w -- ein Invader muss so oft getroffen werden, wie er Spalten breit ist.
  struct GameInvader { int8_t x; uint8_t y; uint8_t w; uint8_t hp; bool alive; };
  struct GameBullet  { int16_t x; uint8_t y; bool alive; };
  static const uint8_t GAME_MAX_INV     = 4;
  static const uint8_t GAME_MAX_BULLETS = 5;
  GameInvader   _gInv[GAME_MAX_INV];
  GameBullet    _gBullet[GAME_MAX_BULLETS];
  uint8_t       _gDefY = 3;                // 0..7, Verteidiger-Zeile
  uint16_t      _gScore = 0;
  uint16_t      _gTopScore = 0;            // Bestwert, ueberlebt Neustarts (siehe begin()/gameOverEnd())
  uint8_t       _gLives = 3;
  unsigned long _gLastTick  = 0;
  unsigned long _gLastSpawn = 0;

  // Bomben (an die 2 physischen Zusatz-LEDs gekoppelt, siehe gameLedState()).
  uint8_t _gBombs          = 2;   // verbleibend; erste Bombe = LED 0, zweite = LED 1
  int8_t  _gBombFlashSteps = 0;   // >0 waehrend der kurzen Explosion (Invader blinken)
  int8_t  _gBombFlashLed   = -1;  // 0/1 = diese LED soll gerade blinken, -1 = keine
  // true ab dem ersten gameStart() dieser Laufzeit, bleibt danach dauerhaft
  // true -- signalisiert gameLedState(), dass ein Spiel die LEDs schon einmal
  // uebernommen hat und sie beim Verlassen von GAME daher explizit ausschalten
  // soll, statt sie unangetastet zu lassen (siehe gameLedState()).
  bool _gLedsOwned = false;

  // Start-Intro: "umgekehrter Pixel-Fill" (volles Bild loest sich zufaellig
  // auf, siehe startGameIntro()/stepGameIntro()) gefolgt von einem einmaligen
  // "PIXEL ATTACK"-Scroll (startGameTitle()), bevor das eigentliche Gameplay
  // beginnt. Nutzt zum Mischen dieselben Puffer wie BOOT_FILL (_animFillIdx/
  // _animFillFrame) -- Boot-Animation und Spiel laufen nie gleichzeitig,
  // eigener Speicher waere reine Verschwendung. Eigener _gIntroActive-
  // Zustand statt _animActive/_animType, damit das Spiel nicht in die fuer
  // die Einstellungen persistierte BOOT_*-Auswahl hineinmischt.
  bool          _gIntroActive = false;
  uint16_t      _gIntroStep = 0;
  unsigned long _gIntroLastStep = 0;

  bool          _gTitleActive = false;
  unsigned long _gTitleStart = 0;
  unsigned long _gTitleDurationMs = 0;
};
