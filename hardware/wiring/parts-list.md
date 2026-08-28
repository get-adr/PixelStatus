# Teileliste — Akkubetrieb (1S-LiPo, No-Shift-Topologie)

Vollständige Teileliste für den Akkubetrieb. Montageanleitung + Verkabelungsblatt:
siehe `hardware/wiring/wiring-sheet.html`.

## Vorhanden (vollständig)

| Teil | Menge | Zweck | Details |
|---|---|---|---|
| 1S-LiPo, 2000 mAh | 1× | Energiespeicher | 51 × 33 × 10 mm |
| TP4057-Lademodul | 1× | Lader + Schutz-IC | Silk „LX-LBES", Pins IN±/BAT±/OUT± |
| LDO-Breakout | 1× | 3,3-V-Versorgung ESP | SOT23-5 „N1F", Silk „V987", 3-Pin Vi/GND/Vo — **Vo = 3,3 V gemessen/bestätigt** |
| Master-Schalter SS-12F15-G070 | 1× | Ein/Aus, gesamtes Gerät | Panel-Mount, für −X-Stirnwand vorgesehen (siehe Gehäuse) |
| ESP8266 D1 mini | 1× | Steuerung | |
| MAX7219-Matrix, 4× | 1 Modul | Anzeige | 32×8 |
| Elko 1000 µF / 10 V | 1× | Matrix-V_CC-Pufferung (Bulk) | Spannungsfestigkeit 10 V gewählt für Reserve über max. 4,2 V LiPo-Spannung |
| 1N5817 | 1× | Rückspeisungsschutz LDO-`Vo` → ESP-`3V3` bei gleichzeitigem USB | bedrahtet, DO-41, 1 A / 20 V, Vf ≈ 0,45 V @ 1 A |
| Keramik-C 100 nF | 2× | 1× Matrix-V_CC-Decoupling, 1× Stabilisierung A0-Spannungsteiler | |
| Widerstand 100 kΩ, 1% | 1× | A0-Spannungsteiler, R1 (Akku `OUT+` → A0) | |
| Widerstand 220 kΩ, 1% | 1× | A0-Spannungsteiler, R2 (A0 → GND) | |
| Status-LEDs 5 mm | 2× | LED-Bucht im Gehäuse | rot + grün (Standardfarben, austauschbar) |
| Vorwiderstand 220–330 Ω | 2× | je 1× pro Status-LED | |
| Gehäuse (3D-Druck) | body + cover + feed_plug | fertig gedruckt, Passung bestätigt (17. August 2026) | siehe `hardware/case/` |
| M2-Schrauben, selbstschneidend | 2× | Schalter-Flansch an −X-Wand | Raster 15 mm, Löcher ø2,2 vorbereitet |

## Werkzeug & Verbrauchsmaterial (für die Montage, nicht Teil der Stückliste)

- Lötkolben + Lötzinn, Entlötlitze/Absaugpumpe für Korrekturen
- Multimeter (Spannung + Durchgangsprüfung — vor jedem Einschalten Pflicht)
- Schrumpfschlauch (mehrere Durchmesser) oder Isolierband für blanke Lötstellen
- Seitenschneider + Abisolierzange
- Kleiner Kreuzschlitz- oder Sechskant-Schraubendreher für die M2-Schalterschrauben
- Etwas dünne Litze (verschiedenfarbig empfohlen: rot/schwarz/gelb) für die kurzen Brücken zwischen den Modulen
- Optional: Heißkleber als Zugentlastung für Kabel an den Modul-Lötpads

## A0-Spannungsteiler — Auslegung

Schaltung: `OUT+` (Akku) → R1 (100 kΩ) → **A0** → R2 (220 kΩ) → GND

Teilerfaktor R2/(R1+R2) = 220/320 = **0,6875**

D1-mini-A0-Limit: ≤ 3,2 V

| Akkuspannung | Spannung an A0 |
|---|---|
| 4,2 V (voll) | 2,89 V |
| 3,7 V (nominal) | 2,54 V |
| 3,0 V (leer) | 2,06 V |

Reserve bei voller Ladung: ~0,3 V unter dem A0-Limit.

Stromverbrauch des Teilers: 4,2 V / 320 kΩ ≈ **13 µA** (vernachlässigbar für die Akkulaufzeit).

## Verdrahtungs-Topologie (Kurzfassung)

1S-LiPo → TP4057 (Lader) → `OUT+`/`OUT−` (SYS-Schiene, ~3,0–4,2 V, geschützt)
→ (a) Matrix-V_CC direkt (+ 1000 µF Elko + 100 nF Keramik-C)
→ (b) LDO (`Vi`) → `Vo` → 1N5817 (Anode am LDO) → ESP `3V3`-Pin
→ Master-Schalter SS-12F15-G070 in der `OUT+`-Leitung (schaltet das gesamte Gerät)
→ A0-Spannungsteiler an `OUT+`/GND, Abgriff an D1-mini-`A0`

Gemeinsame Stern-Masse. SPI unverändert (D7 → DIN, D5 → CLK, D8 → CS). Nicht gleichzeitig USB und Akku speisen lassen — laden über die USB-Buchse des TP4057-Moduls.

Siehe auch `hardware/wiring/wiring-sheet.html` — vollständige Montageanleitung
(Reihenfolge Gehäuse/Löten/Erstinbetriebnahme/Endmontage) inklusive großem
Verkabelungsblatt mit nummerierten Lötschritten (Stand 18. August 2026).
