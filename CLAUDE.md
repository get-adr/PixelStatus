# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Was das ist

Zwei Teile in einem Repo:

1. **Firmware** (`/`, PlatformIO) — **ESP8266** treibt eine **32×8-LED-Matrix aus
   vier MAX7219-Modulen** und zeigt Stati ("On Air", "In a Call"), freien Text,
   Timer oder Uhrzeit. Drei gleichwertige Steuerwege: **Web-UI/HTTP**, **MQTT**
   und **USB-Seriell**. Standalone, Home Assistant nicht nötig.
2. **Companion-App** (`companion/`, Tauri v2) — Tray-/Menüleisten-App für
   Windows/macOS, steuert das Display über HTTP oder USB.

Hardware-Eckdaten: ESP8266 hat **kein Bluetooth** (nur WiFi). Display-Treiber ist
der **MAX7219** (im Projektnamen "Max7291" ist ein Tippfehler).

## Befehle

### Firmware (PlatformIO)

```bash
cp include/config.example.h include/config.h   # einmalig, dann WiFi/MQTT eintragen
pio run                       # kompilieren (Default-Env: nodemcuv2)
pio run -t upload             # flashen
pio device monitor            # serieller Monitor, 115200 Baud
pio run -e d1_mini -t upload  # Variante Wemos D1 mini
```

Es gibt kein Test-Setup; verifiziert wird auf der Hardware bzw. über den seriellen
Monitor. PlatformIO muss ggf. erst installiert werden (`brew install platformio`).

### Companion-App (Tauri v2, `companion/`)

```bash
npm install
npm run tauri icon pfad/zu/icon.png   # einmalig: Icons erzeugen (sonst Build-Fehler)
npm run tauri dev                      # Entwicklungsmodus
npm run tauri build                    # Installer (.dmg / .exe)
```

Braucht **Rust** (`rustup`) zusätzlich zu Node. Voraussetzungen: siehe
`companion/README.md`.

## Architektur

### Firmware

Firmware in `src/`, einzige Build-Unit ist `main.cpp`, das die Module verdrahtet.
Eine Verantwortlichkeit pro Klasse:

- **`DisplayManager`** — einziger Besitzer der MD_Parola/MAX7219-Kette. Kennt die
  Modi `IDLE`, `TEXT_STATIC`/`TEXT_SCROLL`, `TIMER`, `CLOCK`. `loop()` muss in
  jedem Arduino-`loop()` laufen, sonst stehen Scroll-Animation, Timer und Uhr
  still. Der Scroll-Puffer `_buf` ist ein Member, **weil MD_Parola während des
  Scrollens fortlaufend aus diesem Zeiger liest** — kein lokaler String.
  **Timer pausieren** (`pauseTimer()`/`resumeTimer()`, Aktion `timer` mit Wert
  `pause`/`resume` in `Commands.h`, Route `/api/timer?pause=1`/`?resume=1`, Feld
  `timerPaused` im State; UI: Play/Pause/Stop-Icons im Timer-Tab statt der alten
  Countdown/Hochzählen/Stopp-Buttons) friert die Anzeige ein, statt sie zu
  stoppen. `renderTimer()`/`timerRemaining()` berechnen die Restzeit aus
  `millis() - _timerStart`; `pauseTimer()` merkt sich die zum Pausenzeitpunkt
  errechnete Restzeit (`_pausedRemaining`, **vor** dem Setzen von
  `_timerPaused` ermittelt, sonst würde `timerRemaining()` bereits die
  eingefrorene Logik greifen) und beide Methoden brechen bei gesetztem
  `_timerPaused` sofort ab (kein `millis()`-Delta mehr). `resumeTimer()`
  verschiebt `_timerStart` um die Pausendauer nach vorn, statt die Restzeit neu
  zu setzen — dadurch bleiben Sekundenbruchteile exakt erhalten. Gilt für
  Countdown **und** Hochzählen gleichermaßen (Hochzählen hat keine
  `timerRemaining()`, aber dieselbe Einfrier-Logik in `renderTimer()`). Die
  Web-UI sperrt Play/Pause/Stop je nach Zustand (aus/läuft/pausiert) wie ein
  gewöhnlicher Player statt sie dauerhaft aktiv zu lassen.
  **"Pixel Attack"-Gimmick** (Space-Invaders-artig; `GAME`-Mode,
  `gameStart()`/`gameUp()`/`gameDown()`/`gameFire()`, Aktion `game` in
  `Commands.h`, Route `/api/game` in `WebPortal`; UI: eigene "Pixel
  Attack"-Kachel + Panel mit ▲/▼/●-Buttons und Pfeiltasten/Leertaste — der
  Name ist bewusst nicht übersetzt, wie die Matrix-Presets)
  rendert wie die Boot-Animationen direkt per `writeLogicalColumn()`
  (`setColumn()`, orientierungsfrei) statt über Parola/Font — `loop()` bekommt
  dafür denselben frühen `return` wie `_animActive`, sonst würden sich Parolas
  `displayAnimate()`/`renderRotated()` mit den direkten Schreibzugriffen ins
  Gehege kommen. Der Tick (`renderGame()`) ist wie `stepBootAnimation()` per
  `millis()` gedrosselt (~160 ms), kein `delay()`. Verteidiger steht fest am
  rechten Rand und bewegt sich nur in der Zeile (0–7, Ausweich-Mechanik: eine
  Zeile, die kein Angreifer trifft, ist sicher); Angreifer spawnen links mit
  zufälliger Breite (1–3 Spalten, 50/35/15 %, `GameInvader::w` — "unterschiedlich
  große" Gegner; `hp` startet bei `w` und muss auf 0 runtergeschossen werden,
  bevor der Invader wirklich verschwindet — ein breiterer Invader braucht also
  entsprechend mehr Treffer, jeder Treffer zählt einen Punkt, nicht erst der
  finale Kill) und laufen nach rechts, bis zu `GAME_MAX_BULLETS` (5) Schüsse
  gleichzeitig bewegen sich entgegengesetzt (`GameBullet[]`, `gameFire()`
  belegt den ersten freien Slot; ist keiner frei, verpufft der Befehl —
  natürliche Rate-Begrenzung statt eines eigenen Cooldowns). **Kollisionstest
  bewusst nicht per Gleichheit**: Schuss und Angreifer-Vorderkante (`x+w-1`)
  bewegen sich gegenläufig je 1 Spalte/Tick, ihr Abstand ändert sich also um 2 —
  bei ungeradem Startabstand würden sie sich exakt kreuzen, ohne je auf
  derselben Spalte zu stehen (Bsp. Schuss auf 5, Kante auf 4 → nächster Tick
  Schuss 4, Kante 5). Getroffen zählt daher, sobald der Schuss die Vorderkante
  in diesem Tick erreicht ODER überquert hat (`_gBullet[b].x <= front`, nicht
  `==`). Game Over (0 Leben) ruft `scrollMessage()` mit einem einzigen
  durchlaufenden String „GAME OVER   SCORE n   " auf — GAME OVER und der Score
  laufen dadurch scheinbar abwechselnd über die Matrix, ganz ohne eigene
  Umschalt-Logik: die ohnehin bestehende Endlos-Wiederholung von `TEXT_SCROLL`
  (`loop()`/`renderRotated()`) wiederholt einfach den gesamten String.
  `setText()` setzt dabei automatisch `_mode = TEXT_SCROLL`, das Spiel ist
  damit sauber verlassen. `stateJson()` liefert `gameScore`/`gameLives` immer
  mit (auch außerhalb `GAME`, wie schon bei `timerPaused`). Reine
  Web-UI-Steuerung ohne physische Tasten am Gerät, daher kein
  MQTT-/USB-Anwendungsfall für die `game`-Aktion vorgesehen; einen expliziten
  `stop` gibt es bewusst nicht (der übergeordnete Aus-Preset/die generische
  `clear`-Aktion ruft ohnehin `DisplayManager::clear()` und verlässt `GAME`
  damit genauso) — im Panel heißt der frühere Stop-Button daher "Neustart" und
  ruft dieselbe Aktion wie "Start" auf (`gameStart()` setzt ohnehin komplett
  zurück), bleibt anders als ▲/▼/● aber immer klickbar. Die Web-UI taktet
  dafür ihren bestehenden `/api/state`-Poller (statt eines zweiten parallelen
  Timers) auf 400 ms um, solange das Spiel-Panel offen ist, und zurück auf 3 s
  beim Verlassen — die Live-Vorschau (`/api/frame`) läuft unverändert über
  ihre bestehende adaptive Pollingschleife mit, da sie mode-agnostisch aus
  demselben Pixelpuffer liest.
  **Bomben** (`gameBomb()`, Aktion `game bomb`, Taste „B"/Button ✱ im Panel;
  zu Beginn 2 verfügbar, `gameBombs` im State) sprengen alle gerade sichtbaren
  Invader und sind an die **physischen Zusatz-LEDs** gekoppelt (erste Bombe =
  LED 1, zweite = LED 2): **beide LEDs leuchten, solange ihre Bombe noch
  verfügbar ist** (An = verfügbar, ab `gameStart()`), blinken kurz beim
  Zünden und bleiben danach aus (verbraucht) — auf einen Blick sichtbar, wie
  viele Bomben noch übrig sind. Bewusst NICHT über einen direkten
  `LedController`-Zugriff aus `DisplayManager`, sondern über dieselbe
  Entkopplung wie die LED-Countdown-Warnung: `gameLedState(idx)` liefert je
  LED (0/1) einen von vier Zuständen (`GLED_NONE` = Spiel nicht aktiv, LED
  unangetastet; `GLED_AVAILABLE`/`GLED_FLASH`/`GLED_USED`), `main.cpp`s
  `handleGameBombLed()` fragt das in jedem `loop()` für beide LEDs ab und
  wendet `leds.set(idx, …)` nur bei tatsächlichem Wechsel an (Flankenvergleich
  je LED, `s_gameLed[2]`) — `LedController::set()` setzt bei jedem Aufruf mit
  `s != BLINK` auch das globale Wechselblinken zurück, ständiges Aufrufen
  würde das kaputt machen. Die Explosion selbst läuft in `renderGame()`:
  `_gBombFlashSteps` friert für 6 Ticks jede Invader-Bewegung/-Kollision ein,
  zeichnet sie nur in jedem zweiten Tick (Blinken) und löscht sie beim Ablauf
  komplett — kein zusätzlicher Score dafür, um das Punkte-für-Schüsse-Prinzip
  nicht zu entwerten. `gameStart()` setzt `_gBombs` wieder auf 2 zurück.
  **Tickrate** `GAME_TICK_MS` (Invader-/Schuss-Geschwindigkeit) bewusst auf
  220 ms statt der anfänglichen 160 ms — wirkte bei den ersten Tests zu
  hektisch (auch verstärkt durch die unterschiedlich breiten Invader, die
  wegen der Vorderkanten-Prüfung effektiv etwas früher "ankommen").
  **Highscore**: `_gTopScore` wird wie Helligkeit/Ausrichtung in LittleFS
  (`/topscore.txt`) persistiert (`loadTopScore()`/`begin()`), `gameOverEnd()`
  aktualisiert und speichert ihn nur bei einem neuen Bestwert (Flash schonen,
  gleiches Prinzip wie `setBrightness()`) und nimmt ihn — nach `GAME OVER` und
  `SCORE n` — als dritte Phrase (`TOP n`) mit in den Endlos-Scroll auf.
  **Der Leerraum zwischen den drei Phrasen wird bewusst nicht geraten**
  (anfangs 3 Leerzeichen — zu wenig: "SCORE" lief schon ein, bevor "GAME OVER"
  komplett ausgelaufen war, beide zeitweise gleichzeitig sichtbar): `gap` wird
  aus `textWidth(" ")` (tatsächliche Pixelbreite eines Leerzeichens in der
  aktuell gesetzten Schrift) so berechnet, dass er mindestens eine
  Displaybreite (`cols` Spalten) misst — dadurch ist garantiert nie mehr als
  eine der drei Phrasen gleichzeitig im 32×8-Fenster sichtbar.
  **Start-Intro** (`startGameIntro()`/`stepGameIntro()`/`startGameTitle()`,
  aus `gameStart()`): ein "umgekehrter Pixel-Fill" — beginnt beim vollen,
  voll ausgeleuchteten Bild (statt wie `BOOT_FILL` leer/aufbauend) und löscht
  per derselben Fisher-Yates-Mischung zufällig Pixel für Pixel, bis nichts
  mehr leuchtet. Nutzt dafür bewusst dieselben Puffer wie `BOOT_FILL`
  (`_animFillIdx`/`_animFillFrame`) statt eigener — Boot-Animation und Spiel
  laufen nie gleichzeitig, ein zweiter 256-Byte-Puffer wäre reine
  Verschwendung; eigener `_gIntroActive`-Zustand statt `_animActive`/
  `_animType` aber schon, damit das Spiel nicht in die für die Einstellungen
  persistierte Boot-Animations-Auswahl hineinmischt. Danach läuft
  `startGameTitle()` einmalig "PIXEL ATTACK" durch — bewusst über die
  **private** `renderContent()`, nicht über `scrollMessage()`: die würde
  `_mode` auf `TEXT_SCROLL` setzen und damit endgültig aus `GAME`
  herausführen (gewollt beim Game-Over-Bildschirm, hier aber nicht, `_mode`
  muss `GAME` bleiben, damit `renderGame()` danach nahtlos ins eigentliche
  Gameplay übergeht). Da `_mode` währenddessen `GAME` bleibt, treibt
  `renderGame()` selbst `_p.displayAnimate()` (Orientierung 0°) bzw.
  `renderRotated()` (180°) an, statt wie sonst `loop()` — die feste Dauer
  (`_gTitleDurationMs`, aus `textWidth("PIXEL ATTACK") + cols` mal 50 ms/
  Spalte, derselben Rate wie `renderContent()`s Scroll) erspart eine exakte
  Scroll-Ende-Erkennung, die zwischen Parola und dem eigenen 180°-Renderer
  ohnehin unterschiedliche Zykluslängen hätte. Während Intro und Titel
  reagieren `gameUp()`/`gameDown()`/`gameFire()`/`gameBomb()` bewusst nicht
  (sonst könnte z. B. ein zu früh abgefeuerter Schuss bis zum tatsächlichen
  Gameplay-Start unbewegt "einfrieren").
  **Feste Laufrichtung für Titel/Game-Over** (`_gForceLeftScroll`, nur kurz um
  die jeweiligen `renderContent()`-Aufrufe in `startGameTitle()`/
  `gameOverEnd()` gesetzt): erzwingt normale Scroll-Richtung (Eintritt
  rechts) unabhängig von der persistierten `scrollReverse`-Einstellung. Ohne
  das liefe bei umgekehrter Richtung die GAME-OVER/SCORE/TOP-Sequenz sichtbar
  rückwärts (TOP zuerst statt GAME OVER zuerst, da bei `PA_SCROLL_RIGHT`/der
  entsprechenden `renderRotated()`-Richtung das Ende des zusammengesetzten
  Strings zuerst ins Bild kommt). Da Parolas Effekt-Parameter nur beim
  Scroll-*Start* ausgewertet wird (nicht pro Frame), reicht ein kurzes
  Setzen/Zurücksetzen des Flags um den jeweiligen `renderContent()`-Aufruf,
  ohne die persistierte Einstellung selbst anzufassen. Für den eigenen
  180°-Renderer (der `_scrollReverse` bisher pro Frame neu auswertete) gibt
  es dafür jetzt `_rotReverse` — eine bei jedem `renderContent()`-Aufruf neu
  "eingebrannte" Kopie der effektiven Richtung, die `renderRotated()`
  seitdem statt der live einstellbaren `_scrollReverse` liest.
  **`clear()` löschte den Puffer, aber eine noch laufende Scroll-Animation
  lief weiter**: `loop()` rief `_p.displayAnimate()` bisher auch im `IDLE`-
  Modus auf (der Zweig prüfte nur `TIMER`/`CLOCK`/`GAME`, nicht `IDLE`) —
  `displayClear()` leert zwar den Pixelpuffer sofort, stoppt aber nicht
  Parolas bereits konfigurierte Animation. Bei "Aus" mitten in einem langen
  Scroll (z. B. dem GAME-OVER-Text) wurde der Puffer dadurch für einen Frame
  geleert und im nächsten `loop()`-Tick von der weiterlaufenden Animation
  gleich wieder überschrieben — sichtbar als "Aus löscht nicht sofort".
  Fix: `_p.displayAnimate()` läuft jetzt nur noch, wenn `_mode != IDLE`.
  **LEDs beim Verlassen von `GAME` explizit aus** statt nur unangetastet zu
  lassen: `_gLedsOwned` wird mit dem ersten `gameStart()` einmalig `true`
  (bleibt es danach dauerhaft) und lässt `gameLedState()` außerhalb von
  `GAME` `GLED_USED` statt `GLED_NONE` liefern, sobald schon einmal gespielt
  wurde — greift sowohl beim natürlichen Game-Over-Übergang als auch bei
  manuellem "Aus"/`clear()`, ohne dass eine dieser Stellen die LEDs selbst
  anfassen muss. Vor dem allerersten `gameStart()` dieser Laufzeit bleibt es
  bei `GLED_NONE` (unangetastet), damit ein frisch gebootetes Gerät nicht
  ungefragt eventuell manuell gesetzte LEDs abschaltet.
  **Touch-Steuerseite** `/play` (`PLAY_PAGE` in `WebPortal.cpp`, verlinkt über
  einen "Touch-Steuerung"-Button im Spiel-Panel der Startseite,
  `window.open('/play','_blank')` — bewusst ein eigener Tab/eigene Seite statt
  eines weiteren Panels, da die Buttons dafür bildschirmfüllend sein sollen,
  was mit der normalgroßen Startseiten-UI nicht vereinbar wäre) für Handys
  quer gehalten: große Daumen-Buttons links (Hoch/Runter) und rechts
  (Feuer/Bombe), Mitte zeigt Score/Leben/Bomben/Best + Neustart. Nutzt
  dieselbe `/api/game`-Route wie das Desktop-Panel (keine neue Backend-Logik),
  pollt `/api/state` alle 400 ms für die Live-Werte. In Hochformat blendet
  eine `@media (orientation:portrait)`-Regel einen Rotationshinweis ein und
  versteckt die Spalten (`visibility:hidden`, damit ihr Layout-Platz erhalten
  bleibt und beim Zurückdrehen nichts neu aufgebaut werden muss), statt mit
  verrutschter Steuerung spielbar zu bleiben.
  Die Helligkeit wird in LittleFS (`/bright.txt`) persistiert und beim Boot geladen
  (`begin()`/`setBrightness()`), überlebt also Neustarts; `DEFAULT_BRIGHTNESS` ist nur
  der Erstwert. Bei jedem Inhaltswechsel (`setText`, Timer-/Uhr-Start) wird einmal
  `displayClear()` aufgerufen -> sauberer Übergang ohne Überlappung.
  `frameJson()` liest den aktuellen Pixelpuffer (32 Spaltenbytes, Bit r = Zeile r)
  via `getGraphicObject()->getColumn()` aus — Basis der Live-Vorschau (`/api/frame`)
  auf der Startseite. `getColumn(0)` ist die **rechte** Seite, die Web-UI dreht die
  Spalten daher für die Darstellung um.
  **Ausrichtung** (`setOrientation()`, 0/180°, Aktion `orient` in `Commands.h`,
  Route `/api/orientation`, Feld `orientation` im State; UI: Settings → LED-Tab) für
  auf dem Kopf montierte Geräte. Bei 0° rendert wie gehabt Parola direkt (unverändert,
  volle Kompatibilität). Bei 180° übernimmt ein **eigener, kleiner Renderer**
  (`buildRotBuf()`/`renderRotated()`) die Ausgabe: Parolas Scroll-Effekt verschiebt den
  MD_MAX72XX-Puffer inkrementell (`transform(TSL)`, liest den bestehenden Pufferinhalt) —
  ein Überschreiben mit gedrehten Daten würde das korrumpieren. Der eigene Renderer baut
  daher direkt aus den Font-Glyphen (`getChar()`, wie `textWidth()`) einen eigenen
  Spaltenpuffer (`_rotBuf`), schiebt ihn bei Bedarf selbst weiter (eigenes Timing, kein
  Parola) und schreibt pro Frame das um 180° gedrehte 32×8-Fenster direkt via `setColumn()`
  (Spaltenreihenfolge UND Zeilen-Bits je Spalte spiegeln). `frameJson()` liest in diesem
  Fall aus dem eigenen `_lastCols`-Cache statt aus dem (dann unbenutzten) Parola-Puffer.
  Persistiert in LittleFS (`/orient.txt`), wirkt sofort. **90°/270° (Hochkant) waren
  zwischenzeitlich ebenfalls implementiert** — die (c,r)→Panel-Zuordnung der FC16-Module
  entspricht dabei nicht der naiven Annahme (empirisch am Gerät hergeleitet, siehe Git-
  Historie/Kommentare in `renderRotated()`), lieferte aber auch nach Korrektur bei 90°
  kein von 0° unterscheidbares Bild — optisch nicht überzeugend und daher (August 2026)
  wieder entfernt, nur 0/180° unterstützt. **Scrollrichtung** (`setScrollReverse()`, Aktion `scrolldir`
  in `Commands.h`, Route `/api/scrolldir`, Feld `scrollReverse` im State; UI: Settings →
  LED-Tab) unabhängig von der Ausrichtung wählbar (links/rechts), persistiert in LittleFS
  (`/scrolldir.txt`). Wirkt bei 0° über die Parola-Effektauswahl (`PA_SCROLL_LEFT`/
  `PA_SCROLL_RIGHT`), bei 180° über die Laufrichtung von `_rotPos` im eigenen Renderer.
  Scrollender Text läuft dort — wie bei Parola — erst leer ein und am Ende leer aus
  (`cols` Spalten Vor-/Nachlauf um `_rotBuf` herum), statt sofort das ganze Fenster zu
  füllen. **Schriftart**: die eingebaute Systemschrift lässt bei den meisten Zeichen
  Zeile 7 (unterste Reihe) frei (nur Zeichen mit Unterlänge wie g/j/p/q/y nutzen sie) —
  Text „schwebte" dadurch eine Zeile über der Unterkante. `buildShiftedFont()` baut
  deshalb beim Start einmalig eine Kopie der Schrift (ASCII 32–126) mit allen
  Nicht-Unterlängen-Zeichen um 1 Zeile nach unten verschoben (`setFont()`, RAM-Puffer
  `s_shiftedFont` — auf dem ESP8266 kein AVR-PROGMEM-Zwang) und setzt sie für Parola
  **und** den eigenen Renderer gemeinsam (beide nutzen dieselbe `getChar()`-Quelle).
  **Umlaute** (Ä/Ö/Ü/ß/ä/ö/ü): Web-UI/MQTT/USB liefern Text als UTF-8, der Font
  (`MD_MAX72xx_font.cpp`, `USE_NEW_FONT`-Tabelle) indiziert diese Zeichen aber als
  Einzelbyte nach Latin-1 (Codes 196–252). `utf8ToLatin1()` wandelt die zweibytigen
  UTF-8-Folgen (Leitbyte `0xC2`/`0xC3`) vor dem Rendern um; andere Mehrbyte-UTF-8-
  Folgen (Emoji o. Ä.) werden verworfen, da der Font dafür ohnehin keine Glyphen hat.
  `buildShiftedFont()` deckt dafür 32–252 statt nur 32–126 ab (luecken-frei laut
  Fontformat, daher `s_shiftedFont[1300]`). `setText()` haelt **zwei** Strings:
  `_text` (Original-UTF-8, fuer `stateJson()`/die API-Rundreise — ein einzelnes
  Latin-1-Byte waere sonst kein gueltiges UTF-8 und wuerde im Browser als "�"
  landen) und `_renderText` (Latin-1-gewandelt, tatsaechlich gerendert/Breite via
  `textWidth()`). Alle `renderContent()`-Aufrufe (auch nach Ausrichtungs-/
  Scrollrichtungswechsel) nutzen `_renderText`, niemals `_text`. **Laenge
  begrenzt auf `MAX_TEXT_LEN` = 128 Byte** (SEC-06): `_text` wird VOR
  `utf8ToLatin1()` gekuerzt, sodass `_text`/`_renderText` aus derselben
  begrenzten Fassung stammen (nicht der eine aus dem Original, der andere
  aus der Kuerzung). Ohne Grenze liesse sich beliebig langer Text speichern
  -- auf dem ~50-KB-Heap ein Fragmentierungsrisiko, verstaerkt durch die
  sekuendliche Neuserialisierung in `stateJson()` (Web-Poll) und bei jeder
  MQTT-Statusaenderung. Gleiches Prinzip wie `WebPortal::saveDisplayName()`
  (Anzeigename, 40 Zeichen).
  **Einschalt-Animation** (`setBootAnimation()`, Aktion `bootanim` in `Commands.h`
  → `runCommand()`, Route `/api/bootanim`, Feld `bootAnim` im State; UI: Settings →
  System-Tab) füllt die sonst dunkle Lücke zwischen Boot und dem ersten
  `scrollMessage("WiFi...")` in `main.cpp`. Vier Typen (`BootAnim`-Enum: Aus,
  Scan-Wipe, Pixel-Fill, Fortschrittsbalken), persistiert in LittleFS
  (`/bootanim.txt`), Default Scan-Wipe (löst die Lücke auch ohne Zutun für neue/
  aktualisierte Geräte). **Nicht-blockierend**: `startBootAnimation()` setzt nur
  den Zustand, `loop()` treibt die Animation über `stepBootAnimation()`
  schrittweise an (gedrosselt per `millis()`, dasselbe Prinzip wie
  `renderRotated()`) — ein HTTP-Request (Vorschau) kehrt dadurch sofort zurück,
  während die Animation im Hintergrund weiterläuft, ohne Web/MQTT/Seriell zu
  blockieren. Für den **echten** Boot (in `main.cpp`, bevor `net.connectSTA()`
  läuft) wartet ein einfaches `display.loop()`-Schleifchen auf
  `animationActive()==false`, da dort ohnehin noch nichts anderes laufen muss.
  Solange eine Animation aktiv ist, pausiert `loop()` die normale Parola-/
  `renderRotated()`-Wiedergabe komplett (beide schreiben sonst in denselben
  Puffer und würden sich gegenseitig überschreiben). `setBootAnimation()`
  persistiert nur bei Wertänderung, startet aber **immer** neu — Auswahl im
  Dropdown und der „Vorschau"-Button in den Einstellungen lösen dieselbe
  Route/Aktion aus. Alle Typen zeichnen direkt über `setColumn()` (dieselbe
  Technik wie `renderRotated()`, nicht über Parola/Font) via die private
  Helper-Methode `writeLogicalColumn()`, die die 180°-Transformation aus
  `renderRotated()` wiederverwendet — Vollspalten (`0xFF`) sehen in beiden
  Orientierungen gleich aus, der Fortschrittsbalken nutzt Bit 7 (`0x80`, „Zeile 7 =
  unten", dieselbe Konvention wie `buildShiftedFont()`) und landet dadurch
  orientierungsunabhängig am visuellen unteren Rand. **Sonderfall
  Fortschrittsbalken**: einzig dieser Typ ist beim **echten** Boot nicht
  fix-dauernd, sondern an den echten `net.connectSTA()`-Timeout gekoppelt
  (`bootProgressBegin()`/`bootProgressUpdate()`, angetrieben von `pumpDisplay()`
  in `main.cpp` über den bestehenden `pump`-Callback-Mechanismus — läuft dabei
  bewusst NICHT über die Animations-Zustandsmaschine/`_animActive`, und ersetzt
  in `main.cpp` den "WiFi..."-Text während der Wartezeit, statt beide gleichzeitig
  um denselben Pixelpuffer konkurrieren zu lassen); die Vorschau in den
  Einstellungen simuliert ihn stattdessen über `stepBootAnimation()` fest
  getaktet (~1,2 s), da beim Aufruf kein echter WiFi-Connect läuft. Ursprünglich
  gab es zusätzlich `BOOT_FADE` (Helligkeits-Fade-in) und `BOOT_LOGO`
  (Diamant-Bitmap-Reveal) — optisch nicht überzeugend und daher (August 2026)
  wieder entfernt.
- **`Commands.h`** — `runCommand(display, leds, action, value)` ist der **zentrale
  Dispatcher**. Web, MQTT und USB rufen alle hier hinein → ein einziger Ort
  definiert das Verhalten je Aktion (text, scroll, preset, timer, clock, settime,
  brightness, orient, scrolldir, bootanim, led, clear). Neue Befehle nur hier ergänzen. Die Aktion `led`
  erwartet `<1|2|both> <off|on|blink>` und schaltet die Zusatz-LEDs (`LedController`).
- **`LedController`** (`Leds.h/.cpp`) — zwei separat schaltbare Status-LEDs an
  frei wählbaren GPIOs (`LED_PIN_1`/`LED_PIN_2` in `config.h`, default GPIO4/GPIO5),
  unabhängig von der Matrix. Manuell per `led`-Befehl (off/on/blink je LED) und mit
  **automatischer Countdown-Warnung**: `main.cpp` reicht in jedem `loop()`
  `display.timerRemaining()` an `leds.updateCountdown()` — in den letzten
  `LED_WARN_SECS` (5 Min) blinken beide LEDs, in den letzten `LED_WARN_FAST_SECS`
  (1 Min) schneller, bei 0 Dauerlicht. Endet der Countdown, kehren die LEDs in ihren
  manuell gesetzten Zustand zurück (Override-Muster wie `last_manual` der App).
  `loop()` muss laufen, sonst blinkt nichts. `LED_ACTIVE_LOW` für gegen 3V3
  verdrahtete LEDs. Web-UI: LED-Buttons auf der Startseite, Route `/api/led`.
  **Zur Laufzeit konfigurierbar** (Web-Portal `/leds`): Auto-Warnung an/aus,
  Warnfenster, Schnell-Blink-Fenster, Active-Low, **Wechselblinken**
  (`_alternate` — LED 2 blinkt gegenphasig, gilt für manuelles Blinken wie
  Countdown-Warnung; Feld `alt` im State) und **Blinkgeschwindigkeit beim
  manuellen Blinken** (`_blinkPeriod`, ms je voller An/Aus-Zyklus, Default 500,
  Grenzen 100–5000 — gilt nur für manuelles `led X blink`, nicht für die
  Countdown-Warnung, die hat ihre eigenen festen Perioden `_ovPeriod`) und
  **Helligkeit** (`_brightness`, 0–15 — dieselbe Stufenskala wie die Matrix-
  Helligkeit, Default 15 (voll); anders als bei den Blinkperioden gilt sie für
  BEIDE Modi gleichermaßen, auch die Countdown-Warnung; Feld `ledBrightness`
  in `getled`/`cfgled`, Web-UI: Slider auf `/leds`, Companion-App:
  `statusLedBright` im LED-Tab der Geräte-Einstellungen) liegen in LittleFS
  (`/leds.txt`), `config.h`/Konstruktor liefern nur Defaults.
  **Live-Übernahme beim Schieben**: wie die Matrix-Helligkeit (`/api/brightness`
  → `DisplayManager::setBrightness()`) übernimmt auch der LED-Helligkeitsregler
  den Wert sofort beim Loslassen, statt erst beim „Speichern"-Klick der übrigen
  LED-Tab-Felder — vorher war das inkonsistent, da nur die Matrix-Helligkeit
  sofort griff, die LED-Helligkeit aber noch den Speichern-Button brauchte
  (August 2026). `LedController::setBrightness()`
  ist dafür ein eigener, schlanker Setter (persistiert nur bei tatsächlicher
  Änderung, wie `DisplayManager::setBrightness()`), ruft intern aber weiterhin
  `saveConfig()` mit den unveränderten übrigen Werten auf — kein zweites
  Ablageformat nötig. Neue `Commands.h`-Aktion `ledbrightness`, Route
  `/api/ledbrightness?level=`, Slider-`onchange` im Web-UI; die Companion-App
  löst denselben `cfg("ledbrightness", …)`-Aufruf (und, für dieselbe Behandlung,
  `cfg("brightness", …)` für den Matrix-Regler) über `change`-Listener auf
  `ledBright`/`statusLedBright` aus. `saveLed()`/`ledSave()` senden die
  Helligkeit weiterhin zusätzlich mit (harmlos-redundant, da idempotent),
  damit ein reines „Speichern" ohne vorheriges Ziehen der Regler den zuletzt
  geladenen Wert nicht verliert.
  **Der komplette restliche LED-Tab ist inzwischen ebenfalls live** (derselbe
  Speichern-Button-Umweg wurde konsequenterweise auch für die übrigen
  Einstellungen hinterfragt und entfernt, August 2026): Countdown-Warnung an/aus,
  Warnfenster, Schnell-Blink-Fenster, Blinkgeschwindigkeit, Wechselblinken,
  Active-Low und die drei Web-Darstellungsfarben lösen jetzt bei jeder Änderung
  denselben `ledSave()`/`saveLed()`-Aufruf aus, den vorher nur der „Speichern"-
  Button auslöste — der liest bei jedem Aufruf ohnehin den kompletten aktuellen
  Formularstand neu ein, ein einzelnes geändertes Feld sendet also einfach den
  ganzen (bereits vollständig geladenen) Zustand erneut; kein neues Protokoll
  nötig, nur mehr Aufrufzeitpunkte. Der „Speichern"-Button entfällt dadurch in
  Web-UI und Companion-App komplett aus dem LED-Tab (Companion: nur der
  „Laden"-Button bleibt zum manuellen Neu-Einlesen). Anders als bei der
  Helligkeit schreibt `saveConfig()` bei jedem Aufruf bedingungslos die
  komplette `/leds.txt` neu (kein Aenderungscache wie bei `write()`/
  `setBrightness()`) — bei Text-/Zahlenfeldern deshalb bewusst `onchange`
  (Blur/Bestätigen), nicht `oninput` (jeder Tastendruck), um keinen Flash-
  Schreibsturm auszulösen; genau dasselbe Prinzip wie beim Helligkeitsregler.
  **Im System-Tab gilt seitdem dasselbe für den Anzeigenamen**: `nameApply()`
  ruft `/system/save`/`cfgsys` mit ausschließlich `name=…` auf (kein `host`/
  `ntpEnabled`/`ntpServer` im Query) — `applySystemConfig()` prüft jedes Feld
  einzeln über `has()` und lässt fehlende Felder unangetastet, ein reiner
  Namenswechsel triggert also nie versehentlich den Hostname-Neustart-Zweig.
  Hostname und NTP-Einstellungen bleiben bewusst hinter dem „Speichern"-Button
  batched, da ein Hostnamenwechsel einen Neustart auslöst und das nicht bei
  jedem Tastendruck passieren soll.
  Die Helligkeit setzt `write()` erst beim tatsächlichen "an"-Schreiben per
  `analogWrite()` (Software-PWM, `LED_PWM_RANGE`=1023 -- dieser ESP8266-Core hat
  kein `PWMRANGE`-Makro, kein Hardware-Umbau nötig) um — `set()`,
  `updateCountdown()`, `blinkPhase()` kennen weiterhin nur ON/OFF/BLINK, die
  Helligkeit ist rein eine Frage des Pegels bei "an". Bei `activeLow` invertiert
  sich der Duty-Cycle (`LED_PWM_RANGE - duty`); Stufe 15 erzwingt immer den
  vollen Duty (`LED_PWM_RANGE`) — exakt derselbe Pegel wie das vorherige
  `digitalWrite(HIGH)`, reine On/Off-Nutzung (Default) verhält sich also
  unverändert. **`gammaDuty()`**: menschliche Helligkeitswahrnehmung ist grob
  logarithmisch, UND diese LEDs sättigen erfahrungsgemäß schon bei einem
  kleinen Bruchteil des PWM-Bereichs (am Gerät beobachtet: linear ~30 % Duty,
  mit einer ersten Gamma-Korrektur — Exponent 2,2 über den vollen 0..1023-
  Bereich — immer noch derselbe absolute Duty-Wert, nur bei einer anderen
  Reglerstellung; das Auge/die LEDs sättigen dort unabhängig von der
  Kurvenform). Eine reine Potenzkurve über den vollen Bereich kann Dunkel- UND
  Hell-Ende deshalb nicht gleichzeitig gut auflösen. Fix: Stufen 1–14 mappen
  per Gamma-Kurve (Exponent 2,0) auf einen komprimierten, tatsächlich sichtbar
  dimmbaren Bereich `LED_USABLE_MAX_DUTY` (220 von 1023 — Erfahrungswert ohne
  Lichtmessgerät, in zwei Runden von ursprünglich 450 nachjustiert, nachdem
  Stufe 11 noch nicht von Stufe 15 unterscheidbar war; bei Bedarf einfach
  anpassen), Stufe 15 erzwingt separat den vollen Duty. **`write()` schreibt
  nur bei tatsächlicher Änderung**
  (`_lastDuty[2]`-Cache) — `loop()` ruft `write()` bei jedem Tick auf, und
  anders als das vorherige billige `digitalWrite()` setzt `analogWrite()`
  Software-PWM neu auf; ständiges Neusetzen ohne Änderung bremste die
  WiFi-Housekeeping der SDK spürbar aus und machte das Gerät unerreichbar (am
  Gerät beobachtet, August 2026 — Ursache der ersten, fehlerhaften Fassung
  dieser Funktion).
  `saveConfig()` wendet sofort an (kein Neustart, anders als WiFi/MQTT). Pins bleiben compile-time
  (physisch). `combinedStateJson()` in `Commands.h` fügt Display- und LED-State
  (`led1`/`led2`/`warn`) zusammen — genutzt von `/api/state`, `/api/led` und dem
  MQTT-`state`-Topic. **Darstellungsfarben (nur Web-UI):** LED 1 (default rot),
  LED 2 (default grün) und die Matrix-Vorschau (default rot) sind **zur Laufzeit
  konfigurierbar** über Color-Picker auf `/leds`; `WebPortal` persistiert sie in
  LittleFS (`/ui.txt`, Zeilen: led1/led2/matrix/**Anzeigename**; Farben validiert auf
  `#rrggbb`) und liefert sie via `/api/appearance` (inkl. `name`). Der **Anzeigename**
  ist der Titel der Startseite (JS setzt `h1#ttl` + `document.title`). Die Seiten setzen daraus CSS-Variablen (`--led1`/`--led2` +
  gedimmte `--led1d`/`--led2d`) und die JS-Variable `matrixColor` (Live-Vorschau).
  Visualisiert als Live-Statuspunkte neben dem Titel „Pixel Status" (`.dot`,
  Klassen `on`/`blink`/`fast` via `highlightLeds`) und als farbige Swatches in
  Steuer-Buttons und `/leds`-Legende. Die Punkte zeigen die **effektive** Anzeige
  über die State-Felder `p1`/`p2` (`off`/`on`/`blink`/`fast`): während der
  Countdown-Warnung blinken sie also mit (letzte Minute schnell, bei 0 Dauerlicht,
  bei Wechselblinken gegenphasig) — unabhängig vom manuellen `led1`/`led2`, das die
  Buttons hervorhebt. Die Firmware selbst kennt keine Farben (nur GPIO
  an/aus) — Farben sind reine Präsentation. Ebenfalls im Titelbereich: ein
  **Akku-Icon** (`.batt-hd`/`#battHd`, nur sichtbar wenn `battEnabled`), CSS-Icon aus
  `.battIcon` (Rahmen + Nase) und `.battFill` (Breite = Prozentwert, grün `--on`,
  bei `battLow` rot) plus Prozenttext. Eigener Poller `battTick()` gegen `/api/health`
  alle 10 s (Akkuspannung ändert sich langsam, siehe `BatteryMonitor`), pausiert wie
  die anderen Poller bei unsichtbarem Tab (`startPolling`/`stopPolling`).
- **`Presets.h`** — `applyPreset()` mappt Status-Namen (onair, call, …) auf
  Display-Aufrufe. Von `Commands.h` genutzt; neue Presets nur hier ergänzen.
- **`BatteryMonitor`** (`Battery.h/.cpp`) — liest die 1S-LiPo-Spannung über den
  einzigen ADC-Pin (`A0`) via externem Spannungsteiler (R1=100kΩ Akku→A0,
  R2=220kΩ A0→GND; siehe `hardware/wiring/parts-list.md`). Der ESP8266-ADC
  selbst sieht nur 0–1 V; der D1 mini hat dafür bereits einen eigenen
  Onboard-Teiler (100k/220k) zwischen dem `A0`-Pin und dem ADC-Pin des Chips.
  `Battery.cpp` rechnet **beide Teiler kombiniert** zurück auf die Akkuspannung
  (`DIVIDER_FACTOR = 0,6875 · 0,3125`). Ohne verlöteten Teiler liefert `A0` nur
  Rauschen — deshalb ist das Feature über `BATTERY_MONITOR_ENABLED` in
  `config.h` **standardmäßig aus**, bis die Akku-Hardware verlötet ist. Ist es
  aktiv, misst `loop()` gedrosselt (alle 5 s, EMA-geglättet) und berechnet
  Prozent linear zwischen `BATTERY_EMPTY_V`/`BATTERY_FULL_V`. Unterschreitet der
  Wert `BATTERY_LOW_PERCENT`, blendet `main.cpp` einmalig (Flankenerkennung,
  kein Neustart der Scroll-Animation pro `loop()`) „LOW BATT" ein und dimmt über
  `DisplayManager::setBrightnessOverride()` — das setzt nur `MD_Parola`s
  Intensität, **ohne** die in LittleFS persistierte Nutzer-Helligkeit zu
  überschreiben (`clearBrightnessOverride()` kehrt zurück, sobald der Zustand
  wieder verlassen wird — Override-Muster wie bei der LED-Countdown-Warnung).
  Werte fließen in `/api/health` (`battEnabled`/`battVoltage`/`battPct`/
  `battLow`), die Health-Seite zeigt Kachel + Sparkline nur, wenn `battEnabled`
  true ist. Es gibt bewusst **keinen** Ladestatus („lädt gerade"): Das
  TP4057-Lademodul hat keinen dedizierten CHRG-Pin, nur zwei SMD-LEDs — ein
  digitaler Abgriff wäre nur per Board-Rework möglich, wurde verworfen.
- **`WebPortal`** — `ESP8266WebServer` auf Port 80. Liefert die Bedienseite (als
  PROGMEM-String), den generischen `/api/cmd?action=&value=` (für die Companion-
  App) und Komfort-Routen (`/api/preset`, `/api/timer`, …), die alle in
  `runCommand` münden. **`/api/cmd` behandelt zusätzlich Konfig-/Status-Befehle**
  (`get*`/`cfg*`) über `handleConfigCommand`: `value` ist ein URL-kodierter Query-
  String (z. B. `cfgled` → `autoWarn=1&warnSecs=300&led1=%23ef4444…`). Dieselbe
  Methode nutzt die `SerialBridge` → **ein Protokoll über HTTP und USB**. Die
  eigentliche Logik liegt in gemeinsamen `build*`/`apply*`-Helfern, die auch die
  dedizierten Web-Routen (`/leds/save`, `/mqtt/save`, …) verwenden — eine Quelle
  der Wahrheit. Der Parameterzugriff läuft über `Arg`/`Has`-Getter (HTTP:
  `_server.arg`/`hasArg`, USB: geparster Query-String). **Alle Einstellungen liegen auf einer gemeinsamen Seite
  `/settings` mit Registerkarten** (PROGMEM-String `SETTINGS_PAGE`, Tabs System /
  LED / WiFi / MQTT; IDs/Funktionen pro Tab getrennt benannt, Tabs laden lazy beim
  ersten Öffnen; System ist der Default-Tab). Der **System-Tab** konfiguriert den
  Hostnamen (`/system/save` → `NetManager::saveHostname`, Neustart nur bei Änderung),
  den Anzeigenamen (sofort) und die **Uhrzeit**: NTP an/aus + Server (→
  `TimeManager::saveConfig`, sofort) sowie manuelles Stellen (`settime`, deaktiviert
  dabei NTP). `/system/status` (`buildSystemJson`) liefert alles inkl. aktueller
  Gerätezeit + `synced`-Flag. `applySystemConfig` verarbeitet `ntpEnabled`/`ntpServer`/
  `settime` — greift also auch über `cfgsys` (USB/Companion).
  Die Startseite hat dafür nur einen „Einstellungen"-Button. Die alten
  Pfade `/leds`, `/wifi`, `/mqtt` liefern dieselbe Seite und öffnen per Pfad den
  passenden Tab (wichtig für den AP-Setup-Flow `http://192.168.4.1/wifi`). Die
  zugehörigen **API-Routen sind unverändert**: WiFi `/wifi/status`, `/wifi/scan`,
  `/wifi/save` (via `NetManager`, Neustart); MQTT `/mqtt/status`, `/mqtt/save` (via
  `MqttBridge`, Neustart); LED `/leds/status`, `/leds/save` + `/api/appearance`
  (sofort, kein Neustart). Hält dafür `NetManager&`, `MqttBridge&`, `LedController&`,
  `TimeManager&` und `BatteryMonitor&` (nur lesend, für `/api/health`).
  `/wifi/scan` stößt seit FUNC-03 nur noch `WiFi.scanNetworksAsync()` an und
  antwortet sofort (`{"status":"started"}`) — `WiFi.scanNetworks()` blockierte
  vorher `loop()` für die volle Scan-Dauer (mehrere Sekunden), währenddessen
  standen LEDs/Timer/MQTT still. `/wifi/scan/result` wird gepollt (`wifiScan()`
  im Frontend, alle 500 ms bis zu ~10 s), liefert `{"status":"running"}` /
  `"failed"` / `"done"` (mit `networks`), Zustand kommt aus `WiFi.scanComplete()`
  (`WIFI_SCAN_RUNNING`/`WIFI_SCAN_FAILED`/Trefferzahl). Ein bereits laufender
  Scan wird nicht neu gestartet (sonst würde ein zweiter Klick den ersten
  Durchlauf abbrechen).
  Alle Seiten teilen sich ein gemeinsames Stylesheet unter `/style.css` (PROGMEM-
  String `CSS`); Farben laufen über CSS-Variablen, der **Dark Mode** folgt via
  `@media (prefers-color-scheme:dark)` automatisch dem System-Theme und ist per
  Schalter auf der Startseite manuell übersteuerbar (`data-theme` + `localStorage`).
  **UI-Sprache (Deutsch/Englisch)** folgt demselben Muster: ein gemeinsames Skript
  unter `/lang.js` (PROGMEM-String `LANG_JS`) enthält das Wörterbuch `T` (`de`/`en`)
  sowie `applyI18n()`/`setLang()`/`toggleLang()`; Elemente mit `data-i18n`/
  `data-i18n-ph`/`data-i18n-title` werden beim Laden übersetzt, die Wahl liegt rein
  clientseitig in `localStorage` (Default `de`) — kein LittleFS, keine neue
  `get*/cfg*`-Aktion. Umschalter nur auf der Startseite (neben dem Theme-Button,
  `id="lg"`), gilt aber seitenübergreifend, da alle drei Seiten `/lang.js` einbinden
  und denselben `localStorage`-Key lesen. Die Übersetzungsfunktion heißt bewusst
  `tr()` und nicht `t()`, weil die Startseite ein Eingabefeld `id="t"` hat (Eigener
  Text) und `t.value` sonst mit einer gleichnamigen globalen Funktion kollidieren
  würde. Übersetzt werden nur sichtbare UI-Texte — JSON-Feldnamen, MQTT-Topics und
  das Serial-Protokoll bleiben sprachunabhängig; die Matrix-Preset-Texte (ON AIR
  etc.) sind ohnehin fest Englisch und kein UI-Label.
  `/health` (Seite) + `/api/health` (JSON) liefern Laufzeit-Diagnose (freier Heap,
  Fragmentierung, geschätzte CPU-Last via `LoopStats`, Uptime, RSSI, Flash/LittleFS,
  Uhrzeit/NTP, optional Akkuspannung/-prozent via `BatteryMonitor`) mit Live-
  Sparklines im Browser; `/api/sysinfo` ergänzt die
  (statischen) Netzwerk- und Firmware-/Chip-Infos (IP/MAC/BSSID/Kanal, SDK-/Core-
  Version, Build-Zeit, CPU-Takt, echte vs. konfigurierte Flash-Größe). Unterscheidet
  per `apMode`-Feld zwischen Station- und Setup-Hotspot-Betrieb (`NetManager::
  apActive()`): im AP-Fallback sind die STA-Felder (`WiFi.localIP()`/`SSID()`/...)
  leer, da das Gerät dann selbst der Access Point ist — `apMode==true` liefert
  stattdessen `WiFi.softAPIP()`/`softAPmacAddress()`/`apSsid()`/
  `softAPgetStationNum()` (verbundene Geräte); `loadInfo()` im Frontend baut je
  nach Modus unterschiedliche Zeilen.
  **Favicon/PWA-Icons** (`src/Icons.h`, eingebunden von `WebPortal.cpp`):
  `favicon.ico`/`favicon.png`/`apple-touch-icon.png`/`icon-192.png`/
  `icon-512.png` liegen als `PROGMEM`-Byte-Arrays vor (Binärdaten, daher
  über `send_P(code, type, data, len)` mit expliziter Länge ausgeliefert —
  `strlen_P` wie bei den Text-Seiten wäre falsch, PNG/ICO können eingebettete
  Nullbytes enthalten), plus `/manifest.json` für die PWA-Installierbarkeit.
  Alle vier Seiten (`PAGE`/`SETTINGS_PAGE`/`HEALTH_PAGE`/`PLAY_PAGE`) verlinken
  dieselben Routen im `<head>`. Generiert aus dem PixelStatus-Logo: quadratisch
  auf 1024×1024 gepolstert (Transparenz erhalten), mit Pillow (LANCZOS)
  pro Zielgröße herunterskaliert und mit `pngquant --nofs` nachkomprimiert
  (kein Dithering — bei diesem flachen 3-Farb-Motiv kleiner **und** sauberer
  als mit Dithering; die 6 Dateien zusammen ~78 KB Flash). `Icons.h` ist
  komplett generiert, nichts darin wird von Hand gepflegt — bei einem neuen
  Logo denselben Weg wiederholen und die Datei ersetzen. Ob Browser aus dem
  Manifest tatsächlich einen Installieren-Dialog anbieten, hängt zusätzlich
  von HTTPS ab, das dieses Gerät (lokales Netzwerk, kein Zertifikat) nicht
  hat — iOS "Zum Home-Bildschirm" und Androids manuelles "Zum Startbildschirm
  hinzufügen" funktionieren aber auch ohne den automatischen Prompt und nutzen
  dieselben Icons. Die Companion-App nutzt dasselbe Quellbild: einmalig
  quadratisch aufbereitet und per `npm run tauri icon` (siehe
  `companion/README.md`) in `companion/src-tauri/icons/` neu generiert.
- **`NetManager`** — kapselt die WiFi-Verbindung. `connectSTA()` verbindet als
  Client (Creds aus **LittleFS**, Fallback = Defaults aus `config.h`); schlägt das
  fehl, öffnet `main.cpp` per `startAP()` einen **Setup-Hotspot** (`AP_SSID`).
  **Passwort** (`apPassword()`): eigenes `AP_PASSWORD` (≥8 Zeichen, WPA2-Minimum)
  aus `config.h` hat Vorrang; ist es leer/zu kurz (Default seit SEC-05), generiert
  `startAP()` deterministisch ein geräteeigenes 8-stelliges Hex-Passwort aus der
  Chip-ID (`%08X` von `ESP.getChipId()`) — kein weltweit gleiches Standardpasswort
  mehr, kein „offenes Netz"-Fallback für ungültige Werte (anders als zuvor).
  `main.cpp` zeigt AP-Name und -Passwort beim offenen Hotspot **abwechselnd** auf
  der Matrix an (`handleApSetupDisplay()`, alle 6 s, Vertrauensmodell wie ein
  aufgedruckter Router-WLAN-Schlüssel), bis sich ein Gerät verbindet
  (`WiFi.softAPgetStationNum() > 0`) — dann `display.clear()` statt die zuletzt
  gezeigte SSID/das Passwort stehen zu lassen. Hat dabei **Vorrang vor der
  Akku-Warnung** (`handleBatteryWarning()` in `main.cpp` kehrt um, solange
  `net.apActive() && !s_apSetupDone`): sonst würde „LOW BATT" die Anzeige
  überschreiben bzw. deren Dimmung (`setBrightnessOverride()`) das Passwort
  unlesbar machen — genau das wurde am Gerät beobachtet (August 2026, Passwort
  nach ein paar Durchläufen nicht mehr lesbar). Über `/wifi` gespeicherte Creds **und
  Hostname** (`save()` → `/wifi.txt`, 3 Zeilen: SSID/Passwort/Hostname) werden
  beim nächsten Boot genutzt — kein Neu-Flashen. `main.cpp` nutzt `net.hostname()`
  für `WiFi.hostname()`/mDNS; `/wifi/status` liefert die aktuelle Verbindung
  (SSID/IP/Hostname) für die Setup-Seite. Der Hotspot ist reiner Fallback (nur
  aktiv, wenn kein WiFi).
- **`TimeManager`** (`TimeManager.h/.cpp`) — kapselt die Uhrzeit-Sync. NTP an/aus +
  Server liegen in LittleFS (`/ntp.txt`, 2 Zeilen), `config.h` liefert nur Defaults
  (`NTP_SERVER`). Die Zeitzone (`NTP_TZ`) bleibt compile-time und wird in `begin()`
  **immer** gesetzt (`setenv`/`tzset`), damit auch manuell gestellte Zeit lokal
  korrekt angezeigt wird. `loop()` erkennt den WiFi-(Re)Connect (Flanke) und stößt
  NTP dann neu an (`configTime`); ist NTP aus, stoppt `apply()` den SNTP-Client
  (`sntp_stop()` aus `<sntp.h>` — dieselbe Instanz, die `configTime` nutzt), sodass
  eine per `setManual()` gesetzte Zeit stehen bleibt. Zur Laufzeit über den
  System-Tab (`/settings`) konfigurierbar; `main.cpp` ruft `begin()`/`loop()`.
- **`MqttBridge`** — `PubSubClient`. Abonniert `<base>/cmd/#`, publiziert nach
  jeder Änderung retained `<base>/state` (via `combinedStateJson()`, inkl. der
  Zusatz-LED-Felder `led1`/`led2`/`warn`). **Zur Laufzeit konfigurierbar** (Web-
  Portal `/mqtt`): enabled/Host/Port/User/Passwort/Base-Topic liegen in LittleFS
  (`/mqtt.txt`), `config.h` liefert nur Defaults. Ist MQTT aus oder kein Host
  gesetzt, tut `loop()` nichts. Reconnect ist nicht-blockierend (5-s-Intervall);
  `setSocketTimeout(2)` sorgt dafür, dass ein **unerreichbarer Broker die `loop()`
  – und damit die Web-UI – nicht mehr lahmlegt** (früher blockierte der Default-
  Timeout von 15 s). PubSubClient braucht einen freien C-Callback → der Datei-
  statische Zeiger `s_self` führt zurück zur Instanz (es gibt nur eine).
  **Home-Assistant-MQTT-Discovery** (auf Wunsch nach einer Autoconfig für
  Home Assistant ergänzt, August 2026, Umfang „Vollständig" gewählt): bei
  jedem (Re-)Connect veröffentlicht `publishDiscovery()`
  retained Config-Nachrichten unter `homeassistant/<komponente>/pixelstatus_<chipid>/
  <objekt>/config` — HA legt daraus automatisch Entitäten an, gruppiert unter einem
  gemeinsamen Gerät (`deviceBlock()`, Identifier = Chip-ID, bleibt über Neustarts
  stabil, keine Duplikate). Elf Entitäten: `select` Status (Presets, **optimistic
  ohne state_topic** — der Anzeigeinhalt lässt sich nicht verlustfrei auf einen
  Preset-Namen zurückrechnen, z. B. bei eigenem Text oder der Uhr, HA merkt sich
  daher nur die zuletzt gesendete Auswahl), `text` für eigenen Text, `number` für
  die Matrix-Helligkeit, je ein `light` für die zwei Zusatz-LEDs, drei
  Diagnose-`sensor` (Heap/RSSI/Laufzeit) + zwei weitere nur bei aktiviertem
  `BatteryMonitor` (Spannung/Prozent), ein `binary_sensor` für die
  Countdown-Warnung. Bis auf den Select lesen **alle** ihren State per
  `value_template`/`value_json.<feld>` aus zwei gemeinsamen, retained State-Topics
  — `<base>/state` (bereits vorhanden, `combinedStateJson()`) und neu `<base>/health`
  (`{"heap":…,"rssi":…,"uptime":…[,"battVoltage":…,"battPct":…]}`) — kein eigenes
  State-Topic pro Entität nötig. `<base>/state` wird dafür jetzt **auch außerhalb
  eigener MQTT-Kommandos** periodisch (1 s gedrosselt, inhaltlich diff-geprüft wie
  `LedController::write()`) neu veröffentlicht (`publishState()`), sonst blieben
  HA-Entitäten nach einer Web-UI-/USB-Änderung veraltet stehen; `<base>/health`
  ebenso, aber mit 15-s-Drosselung (`publishHealth()`). `LedController::stateFields()`
  liefert dafür neu auch `ledBrightness` (vorher nur in `buildLedStatus()`/`getled`).
  **Verfügbarkeit**: Last Will (`<base>/availability`, "offline", retained) direkt
  im `connect()`-Aufruf gesetzt, `reconnect()` publiziert "online" nach erfolgreichem
  Connect — sonst blieben Entitäten nach einem Absturz/Stromausfall fälschlich
  "verfügbar" mit veraltetem Stand.
  **Zusatz-LEDs als Licht — bewusst NICHT das JSON-Schema** (`schema:"json"`,
  state+brightness+effect atomar in einer Nachricht): am echten Broker als
  `KeyError:'state'` im HA-Log aufgefallen — HA erwartet dafür direkt `{"state":…}`
  auf dem State-Topic, ein per `value_template` aus `<base>/state` umgebauter Wert
  wird von diesem Schema nicht unterstützt. Stattdessen das **Standard-Lichtschema**:
  ein eigenes Kommandotopic je Fähigkeit (`cmd/led1`, `cmd/led1brightness`,
  `cmd/led1effect`, analog `led2`) mit schlichter Textnutzlast (kein JSON), State
  wie bei den Sensoren per `value_template` aus `<base>/state`. Braucht zusätzlich
  ein explizites `"supported_color_modes":["brightness"]` in der Discovery-Config —
  ohne das registriert HA das Licht nur als reines On/Off-Gerät (`["onoff"]`), auch
  wenn `brightness_command_topic`/`brightness_state_topic` gesetzt sind (ebenfalls
  am echten Broker beobachtet, in der offiziellen Doku für das Standardschema nicht
  klar dokumentiert). `brightness_scale:15` lässt HA direkt die native 0–15-Stufenskala
  senden/zeigen, keine 0–255-Umrechnung im Code nötig. Helligkeit ist geräteweit
  gemeinsam (`LedController::_brightness` gilt für beide LEDs, siehe `Leds.h`) — ein
  Helligkeitsbefehl an eine der beiden HA-Lampen wirkt automatisch auf beide, genau
  wie der gemeinsame Regler im Web-UI/der Companion-App. Da HA state/brightness/
  effect bei einem kombinierten `light.turn_on(brightness=…, effect="blink")`-Aufruf
  als **separate** Nachrichten auf getrennten Topics ohne garantierte Reihenfolge
  schickt, würde ein bedingungsloses „ON" bei `cmd/led1` ein gleichzeitig gesetztes
  `effect="blink"` je nach Ankunftsreihenfolge wieder zurücksetzen (am echten
  Broker reproduziert). Fix: „ON" wird nur angewendet, wenn die LED zuvor wirklich
  aus war (`LedController::manualState()`, neuer Getter); war sie schon an oder am
  Blinken, bleibt sie es, und ein separates/gleichzeitiges `effect="blink"` gewinnt
  unabhängig von der Nachrichtenreihenfolge. „OFF" setzt weiterhin bedingungslos aus.
  Der PubSubClient-Puffer wird in `begin()` auf 1024 Byte vergrößert
  (`setBufferSize()`) — der Default (256 Byte) reicht für die Discovery-Configs
  (Geräte-Block + Lichter mit `brightness_scale`/`effect_list`) nicht, überlange
  Nachrichten würden sonst **still** verworfen. Ganze Implementierung gegen eine
  echte Home-Assistant-Instanz verifiziert (MCP-Zugriff): Entitäten erscheinen
  automatisch, Status/Text/Matrix-Helligkeit/Lichter (inkl. kombiniertem
  Helligkeit+Blink-Befehl) in beide Richtungen getestet, HA-Fehlerlog vor
  Abschluss auf Sauberkeit geprüft.
- **`SerialBridge`** — liest zeilenweise `<action> <value>` von UART0 (USB) und
  antwortet mit einer JSON-Zeile. Steuerbefehle → `runCommand`; **Konfig-/Status-
  Befehle** (`get*`/`cfg*`) → `WebPortal::handleConfigCommand` (dieselbe Logik wie
  `/api/cmd`) → so lassen sich **alle Einstellungen inkl. WiFi/MQTT auch über USB
  provisionieren**. Teilt sich den Port mit den Debug-Logs (115200 Baud); die App
  filtert JSON-Zeilen (`{…}`). Hält dafür `WebPortal&`.

Datenfluss: Web/MQTT/USB → `runCommand` → DisplayManager-Methode → `stateJson()`
als Antwort/State. Die Transporte kennen sich nicht; einziger gemeinsamer Zustand
ist der `DisplayManager`. Uhrzeit verwaltet der **`TimeManager`**: per NTP (nur mit
WiFi, Server/An-Aus zur Laufzeit über den System-Tab) oder manuell per `settime
<epoch>` (USB oder System-Tab, deaktiviert dabei NTP). Ohne WiFi/NTP und ohne RTC
geht die Zeit bei jedem Neustart verloren.

Effizienz in heißen Pfaden: `renderClock()` drosselt die Zeitabfrage auf ~2×/s
(statt jeder `loop()`-Iteration → kein `localtime()`-Sturm), die Web-Vorschau
(`/api/frame`) pollt **adaptiv** (schnell nur bei bewegtem Bild, sonst ~1×/s), und
alle Browser-Poller (state/frame/health/mqtt) **pausieren bei unsichtbarem Tab**
(`visibilitychange`).

### Companion-App (`companion/`)

Tauri v2: Rust-Backend + statisches HTML/JS-Frontend (über `withGlobalTauri`, also
`window.__TAURI__` ohne Bundler). Das Fenster **markiert den aktiven Status**
(On Air/Call/…): `send()` wertet die JSON-Antwort aus und `refreshState()` pollt
alle 3 s (nur bei sichtbarem Fenster, via `send_config` „state") — spiegelt so auch
den Auto-Status (Mikrofon) und externe Änderungen.

- **`src-tauri/src/lib.rs`** — Tray-Menü, Tauri-Commands (`get_settings`,
  `save_settings`, `list_serial_ports`, `send_command`, `send_config`,
  `set_language`), App-State. Auf macOS `app.set_activation_policy(Accessory)`
  gleich zu Beginn von `setup()` — reine Tray-App ohne Dock-Icon/Cmd+Tab-Eintrag
  (sonst Tauri-Standard `NSApplicationActivationPolicyRegular`, wie eine normale
  Fensteranwendung). Windows kennt dieses Konzept nicht (App-Icon nur im Tray,
  kein Taskleisten-Eintrag ist dort ohnehin Standard bei einer reinen Tray-App).
  (`Mutex<Settings>`). Tray-Klicks und Fenster teilen sich diesen State.
  **Wichtig:** in `send_command`/`send_config` wird der Mutex geklont und sofort
  freigegeben — nie über ein `await` gehalten. `send_command` merkt `last_manual`
  (für den Auto-Status), `send_config` **nicht** (Konfig ist kein Status).
- **`src-tauri/src/transport.rs`** — `dispatch()` wählt nach `settings.transport`
  zwischen HTTP (`reqwest` → `/api/cmd`) und USB (`serialport` → `<action> <value>\n`).
  Die **Geräte-Einstellungen** (System/LED/WiFi/MQTT im Fenster) nutzen dasselbe
  `get*`/`cfg*`-Protokoll über `send_config` → funktionieren also **über USB und
  HTTP gleichermaßen** (auch WiFi/MQTT-Provisionierung per USB). `send_serial`
  liest bis zur Antwortzeile und filtert die JSON-Zeile heraus (Port teilt sich
  ggf. mit Debug-Logs). Das reine Öffnen des Ports löst am D1 mini **keinen** Reset
  aus (nur die esptool-DTR/RTS-Sequenz), USB-Steuerung läuft also ohne Reboot. Ein
  Datei-statischer `SERIAL_LOCK` serialisiert alle Port-Zugriffe (Fenster, Tray,
  Auto-Status, Tray-Poller öffnen ihn sonst evtl. gleichzeitig → „port busy").
- **`src-tauri/src/settings.rs`** — Einstellungen als JSON im App-Config-Verzeichnis.
  **`language`**-Feld (`"de"`/`"en"`, `#[serde(default)]` auf `"de"` für alte
  `settings.json` ohne das Feld) haelt die UI-Sprache des Fensters fest — anders
  als bei der Firmware (dort clientseitig in `localStorage`) hier im Rust-Backend
  persistiert, weil die Sprache auch das **Tray-Menü** betrifft (siehe unten), das
  kein DOM/`localStorage` hat.
- **`src-tauri/src/mic.rs`** — `mic_in_use()` plattformabhängig: macOS via CoreAudio-
  FFI (`kAudioDevicePropertyDeviceIsRunningSomewhere`, **keine** Mikrofon-Berechtigung
  nötig), Windows via Registry (`ConsentStore`), sonst `false`.

**Auto-Status** (`spawn_auto_status_watcher` in `lib.rs`): OS-Thread pollt alle 2 s
`mic::mic_in_use()`, schaltet bei Aktivität auf `preset call` und stellt beim
Auflegen `last_manual` wieder her. Deshalb merkt sich `AppState.last_manual` jeden
**manuell** gesetzten Befehl (in `send_command` und im Tray-Handler) — Auto-Befehle
aktualisieren ihn bewusst nicht. Der Watcher nutzt `block_on`, da er kein async-Task ist.
**Auch der Tray-Handler** (`handle_menu_event`) dispatcht Status-Klicks über einen
eigenen OS-Thread + `block_on` — nicht via `tauri::async_runtime::spawn`, da der
**blockierende USB-Transport** sonst einen Runtime-Worker blockieren würde (das war
der Grund, warum Tray-Status-Klicks nicht ankamen).

**Tray-Status-Markierung** (`spawn_tray_status_poller`): Die Status-Einträge sind
`CheckMenuItem`. Ein OS-Thread pollt alle 5 s den Gerätezustand (`send_config`
„state", auch bei verstecktem Fenster) und setzt per `run_on_main_thread` das
Häkchen am passenden Eintrag (`active_status_id` mappt mode/text → Item-Id).
Spiegelt so auch externe Änderungen (Web/MQTT/anderer Client). Die Items liegen im
`AppState`, damit `handle_menu_event` bei einem Tray-Klick das Häkchen **sofort**
korrekt setzt (sonst zeigt macOS durch den `CheckMenuItem`-Auto-Toggle bis zum
nächsten Poll kurz zwei Haken).

**UI-Sprache** (`src/main.js`): dasselbe `data-i18n`/`data-i18n-ph`/`tr()`/
`applyI18n()`-Muster wie die Firmware (siehe `WebPortal`/`lang.js`), aber eigenes
Wörterbuch `T` (kein Codesharing zwischen den Projekten). Die Sprache kommt beim
Start aus `get_settings().language`, ein `<select id="lang">` in der
„Verbindung"-Sektion ruft `set_language` (neuer Tauri-Command) auf, das
`settings.rs` persistiert **und** die Tray-Beschriftungen live umsetzt (siehe
unten). Der Treiber-Hinweis (`driverHint`) enthält `<a>`-Tags; da `applyI18n()`
diesen Absatz per `innerHTML` neu schreibt, werden die Klick-Listener der Links
danach über `attachExternalLinks()` neu gebunden (sonst tot nach einem
Sprachwechsel). Beim Speichern der Verbindungseinstellungen (`saveSettings`-
Button) wird `language` in den gesendeten `Settings`-Wert übernommen, sonst würde
`save_settings` es (serde-Default `"de"`) überschreiben.

**Tray-Menü-Sprache**: `settings_item`/`quit_item` liegen — wie `status_items` —
im `AppState`, damit `apply_tray_language()` per `set_text()` umbenennen kann
(kein Menü-Rebuild). `tray_label(id, lang)` übersetzt nur die tatsächlich
deutschen Einträge (Uhrzeit/Eigener Text/Aus/Einstellungen/Beenden) — On Air/In a
Call/Busy/BRB sind Presetnamen und bleiben in beiden Sprachen Englisch. Wird beim
Start (gespeicherte Sprache) und bei jedem `set_language`-Aufruf angewendet.

Die App ist ein reiner Client der Firmware-API — Steuer-Aktionsnamen müssen mit
`src/Commands.h` übereinstimmen, die Konfig-/Status-Aktionen (`get*`/`cfg*`) mit
`WebPortal::handleConfigCommand`.

## Konventionen

- **Zugangsdaten / Pins** stehen in `include/config.h` (gitignored). Vorlage und
  Doku der Defines: `include/config.example.h`. Niemals echte Credentials in
  versionierten Dateien.
- Hardware-SPI ist fest: **DIN = D7/GPIO13, CLK = D5/GPIO14**. Nur **CS** ist über
  `CS_PIN` konfigurierbar. Modulanzahl über `MAX_DEVICES`. Die zwei Zusatz-LEDs
  hängen an `LED_PIN_1`/`LED_PIN_2` (default GPIO4/D2 und GPIO5/D1, beide frei ohne
  Boot-Nebenwirkungen); `LED_ACTIVE_LOW` invertiert den Pegel.
- Erscheint die Matrix gespiegelt/verschoben: `HARDWARE_TYPE` in
  `DisplayManager.cpp` von `FC16_HW` auf `GENERIC_HW`/`PAROLA_HW` ändern.
- MQTT wird zur **Laufzeit** über `/mqtt` ein-/ausgeschaltet und konfiguriert
  (persistiert in LittleFS); `MQTT_ENABLED`/`MQTT_*` in `config.h` sind nur noch
  Defaults. `MqttBridge` wird **immer** einkompiliert (kein `#if` mehr in
  `main.cpp`) und im `loop()` per Runtime-Flag gated. Die Web-UI bleibt unberührt.
- In der Companion-App nie den Settings-`Mutex` über ein `await` halten. Frontend
  ohne Bundler — Imports laufen über `window.__TAURI__`, nicht über `@tauri-apps/api`.
- **Keine wörtlichen Zitate, Namensnennungen oder tagesgenauen Daten in Kommentaren
  oder Doku** (Quellcode wie `CLAUDE.md` gleichermaßen). Die Begründung hinter einer
  Entscheidung („Warum") gehört rein, aber unpersönlich in dritter Person formuliert
  (z. B. „auf Nutzerwunsch ergänzt", „optisch nicht überzeugend und daher entfernt")
  statt als `Name: „…"`-Zitat. Reicht Monat/Jahr für die zeitliche Einordnung, kein
  Tagesdatum.
