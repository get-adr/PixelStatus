// ================================================================
//  Max7219 32x8 Display - Gehaeuse fuer Akkubetrieb
//  Druck: Bambu Lab P1S, PLA.   Einheiten: mm.
//
//  Zwei Teile:
//    body  = Front-Blende + Wanne in einem (Bauteile von HINTEN laden)
//    cover = Rueckdeckel (klippt/reibt ein)
//
//  ---> ALLE mit (*) markierten Masse mit dem Messschieber pruefen
//       und hier oben anpassen. Danach neu rendern.
//  ----------------------------------------------------------------
//  Druck-Orientierung: body mit der FRONT nach UNTEN drucken
//  (Fenster auf dem Druckbett) -> saubere Front, keine Stuetzen.
//  Der Lader-Halter hat eine 45-Grad-Schraege und druckt so mit.
// ================================================================

part = "both";     // [body, cover, both]  -> was gerendert/exportiert wird

$fn = 64;

// ---- Matrix-Modul (FC-16 4-in-1) ------------------------------
disp_w  = 131;   // * Laenge des Moduls (lange Kante)     [gemessen]
disp_h  = 32;    // * Hoehe des Moduls (kurze Kante)      [gemessen]
disp_d  = 14;    // * Tiefe inkl. Rueckseiten-Header      [gemessen]
led_ov  = 0.5;   //   Ueberlappung Blende<->Modul pro Seite. LEDs bis zum Rand,
                 //   daher minimal. Matrix wird durch mfit eng gefuehrt, damit
                 //   der schmale Rand rundum fasst. Halt zusaetzlich uebers
                 //   Sandwich (Deckel druckt Bauteile gegen die Matrix).
                 //   -> beim naechsten Coupon feinjustieren (0.4-0.6).

// ---- LED-Bucht (2x 5mm LED neben der Matrix, -X Seite) --------
// Verbreitert den Innenraum in X: zwischen der linken (-X) Stirnwand und
// der Matrix entsteht eine Bucht. Zwei 5-mm-LEDs sitzen VERTIKAL gestapelt,
// Presssitz durch die Front. Eine duenne Abstandhalter-Rippe (spacer) trennt
// Bucht und Matrix und bildet den -X Endanschlag der Matrix (die vorher
// beidseitig eng gefuehrt war). Rueckraum bleibt frei fuer die LED-Verdrahtung.
led_en    = true;   // LED-Bucht an/aus (aus -> Gehaeuse wie vorher)
led_bay_w = 20;     // * Breite der Bucht in X (innen)
led_spacer = 2.0;   //   Dicke der Abstandhalter-Rippe (Bucht <-> Matrix)
led_spacer_drop = 3; //  Rippe endet X mm vor der Matrix-Rueckseite (Z) -> Spalt
                     //   fuer seitlich abstehende Matrix-Anschluss-Pins
led_d     = 5.2;    // * Loch-Durchmesser fuer 5mm LED (Presssitz; ggf. 5.0-5.4)
led_pitch = 14;     //   Y-Abstand der beiden LED-Mitten (um OY/2 zentriert)

// ---- Rueckraum (Akku / D1 mini / Lader liegen hier) -----------
comp_depth = 20; // * Tiefe hinter der Matrix = dickstes Bauteil + Reserve
                 //   (D1 mini mit Stiftleisten ~12 mm; Akku 10 mm -> 14 = Luft).
                 //   Auf 20 erhoeht, damit die 2 Aufwickel-Windungen sauber in
                 //   den Rueckraum passen (Zb = front+disp_d+comp_depth = 36 mm).

// ---- Akku (Pouch, 2000 mAh, 51 x 33 x 10) ---------------------
batt_w  = 33;    // * Mass laengs Y (=Hoehe 33; bestimmt die Gehaeusehoehe) [gemessen]
batt_t  = 10;    // * Dicke (in Z; nur Reserve-Check)                       [gemessen]
// (Laenge 51 mm laeuft in X mit, unkritisch)

// ---- USB-C Ladeplatine (TP4057) -------------------------------
chg_l   = 17;    // * Mass in X (nacktes PCB; Tiefe D)              [gemessen]
                 //   Achtung: die Tasche muss ~21 mm fuellen (PCB + USB-C-Ueberstand);
                 //   der Ueberstand steckt in chg_extra, nicht in chg_l.
chg_w   = 12;    // * Mass in Y an der Stirnwand (Breite B)         [gemessen]
chg_t   = 1.4;   // * Platinendicke (geschaetzt - ggf. messen)
// USB-C Oeffnung = Stadion-Form (Rechteck mit voll verrundeten Enden).
// Stecker-Metallschale ist ~8,34 x 2,56 mm; diese Werte = Schale + wenig Spiel.
usbc_w  = 9.0;   // * Breite der Oeffnung (Aussenmass Stadion)
usbc_h  = 3.2;   // * Hoehe der Oeffnung  (= Durchmesser der Rundungen)
usbc_zc = 2.4;   // * Mitte der Buchse ueber Taschenboden (chg_z0). 1.7 -> 2.2 -> 2.4:
                 //   1.7->2.2 = Buchse lag ~0.5 mm hoeher als das Loch. 2.2 passte knapp,
                 //   +0.2 mehr Richtung Rueckseite (+Z) fuer etwas Luft. cz waechst mit.
// Feinabstimmung Stecker-Sitz:
usbc_cb_w = 12.0; //   Option 1: Senkung aussen (fuer Stecker-Koerper) Breite
usbc_cb_h = 6.0;  //   Option 1: Senkung Hoehe
usbc_cb_d = 1.0;  //   Option 1: Senkung Tiefe (Restwand = wall - usbc_cb_d - chg_xshift)
chg_xshift = 0.5; //   Option 2: Platine ragt so weit in eine Innenwand-Aussparung

// ---- Test-Coupon ----------------------------------------------
coupon_len = 32;  //   Laenge des +X-Endstuecks fuer den Testdruck

// ---- Schalter SS-12F15-G070 (Schieber in der -X-Stirnwand, geg. Ladeteil) ----
// Geflanschter SPDT-Schieber (Datenblatt G-Switch, Masse aus der Zeichnung).
// Montage: Flansch mit 2 Schraubloechern liegt INNEN an der -X-Wand; der Schieber
// tritt durch einen Schlitz nach aussen (gleitet in Y/senkrecht), Koerper + Pins
// ragen nach innen (+X). Befestigung: 2x M2-Schraube durch den Flansch in die Wand.
// Zone Z~17-23 (hinter Matrix 16, vor Kabel-Feed ~24). (*) = am Teil nachmessen.
switch_enable = true;
sw_plate_l = 19.5;  //   Flansch-Laenge (Ohr-zu-Ohr, Y)
sw_plate_w = 5.6;   //   Flansch-Breite (Z)
sw_plate_t = 0.4;   //   Flansch-Dicke
sw_hole_sp = 15.0;  //   Schraubloch-Abstand Mitte-Mitte (Y)
sw_body_l  = 10.5;  //   Koerper-Laenge (Y)   [Info/Kollision]
sw_body_w  = 4.6;   //   Koerper-Breite (Z)   [Info/Kollision]
sw_body_h  = 5.0;   //   Koerper-Hoehe unter Flansch, ragt nach innen (X) [Info]
sw_act_w   = 3.0;   //   Schieber-Knopf Breite (Z)
sw_act_l   = 4.0;   // * Schieber-Knopf Laenge (Y) -- am Teil pruefen
sw_travel  = 3.0;   //   Schieber-Weg (Y)
sw_act_h   = 7.0;   //   Schieber-Hoehe ueber Flansch (G070) [Info]
sw_fit     = 0.4;   //   Passungsspiel
sw_screw_d = 1.7;   //   Kernloch fuer M2 (selbstschneidend) in die Wand
sw_zc      = 20;    //   Z-Mitte (frei zw. Matrix 16 und Feed ~24)

// ---- Gehaeuse allgemein ---------------------------------------
wall    = 2.4;   // Wandstaerke Body
front   = 2.0;   // Dicke der Front-Blende
covwall = 2.0;   // Dicke Rueckdeckel
fit     = 0.4;   // Passungsspiel (Bauteile allgemein)
mfit    = 0.25;  // Passungsspiel Matrix in Y (eng -> zentriert, damit Rand fasst)
mfit_x  = 0.6;   // Passungsspiel Matrix in X pro Seite (Laenge). Groesser als mfit,
                 //   weil ueber die 131 mm der XY-Schwund + Messtoleranz die enge
                 //   0.25 auffrisst -> Slot war minimal zu kurz. Sandwich haelt trotzdem.
chg_fit = 1.2;   // Passungsspiel Lader-Halter (locker -> Platine faellt leicht rein)
covfit  = 0.25;  // Passungsspiel Deckel-Lippe (P1S: 0.2-0.3)
corner  = 3.0;   // Aussen-Eckenradius

// ---- abgeleitete Masse (nicht anfassen) -----------------------
led_bay = led_en ? led_bay_w  : 0;        // effektive Bucht-Breite
spacer  = led_en ? led_spacer : 0;        // effektive Abstandhalter-Dicke
mat_slot_w = disp_w + 2*mfit_x;           // X-Slot der Matrix (Laenge; etwas lockerer)
inner_w = led_bay + spacer + mat_slot_w;  // X innen (Bucht + Rippe + Matrix)
inner_h = max(disp_h + 2*mfit, batt_w + 2*fit); // Y innen (Akku ODER Matrix)
inner_d = disp_d + comp_depth;            // Z innen (Front->Deckel)

matrix_x0 = wall + led_bay + spacer;      // -X Kante des Matrix-Slots

OX = inner_w + 2*wall;   // Aussen X
OY = inner_h + 2*wall;   // Aussen Y
Zb = front + inner_d;    // Body-Tiefe (bis Deckelkante)

win_w = disp_w - 2*led_ov;   // Fenster
win_h = disp_h - 2*led_ov;
win_x0 = matrix_x0 + mfit_x + led_ov;   // -X Kante Fenster (mit Matrix ausgerichtet)

// Lader-Halter: rechte Stirnwand (+X), Platine in Y zentriert.
// WICHTIG: Alles bleibt HINTER der Matrix (Z > disp_d), damit die Matrix
// mit ihrem rechten Ende noch reinpasst. Abstuetzung: Stirnwand + Fins.
chg_gap  = 1.2;                      // Luft zwischen Matrix-Rueckseite und Platine (Z).
                                     //   Von 2.0 auf 1.2 -> Platine faellt 0.8 mm tiefer.
                                     //   Das ist das MAX: Bodenplatte-Unterkante liegt dann
                                     //   bündig an der Matrix-Rueckseite (Z=16). Nicht weiter.
chg_z0   = front + disp_d + chg_gap; // Platinen-Unterseite (Z)  ~18
chg_y0   = (OY - chg_w)/2;           // Platinen -Y Kante
chg_extra = 2.0;                     // zusaetzliche Tiefe (X) der Ladetasche.
                                     //   sx = chg_l+chg_fit+chg_extra = 17+1.2+2.0 = 20.2 mm.
                                     //   Zurueck auf den Originalwert: die frueheren Symptome
                                     //   ("zu kurz", "1 mm locker") kamen NICHT von der Laenge,
                                     //   sondern von der 0.5 mm zu hohen USB-C-Buchse (siehe
                                     //   usbc_zc). Sitzt die Buchse im Loch, fixiert der Stecker
                                     //   die X-Position -> 3.2 mm Luft in der Tasche sind egal.

// ---- ESP-USB Docking-Kabel (Micro-USB, fest) + Aufwickel-Nut ----
// Umlaufender KRAGEN (Gehaeuse wird groesser) mit EINGELASSENER, RUNDER Nut:
// Kabel liegt versenkt -> abgewickelt glatte Seiten, liegt flach auf.
// Runde Nut = Kreis-Cutter entlang des Umfangs gezogen -> verrundete Kanten.
wind_en    = true;           // Kragen + Nut an/aus
cable_d    = 4.2;            // * Kabeldurchmesser + etwas Luft
shell      = 4.6;            // Kragenstaerke (umlaufend)  [muss >= Nut-Tiefe = cable_d]
wind_zback = 3.6;           // Abstand der Nutmitte von der Rueckkante
gr_rc      = cable_d/2 + 0.7;// Cutter-Radius (Rundung der Nut)
gr_e       = cable_d - gr_rc;// Versatz nach innen -> Nut-Tiefe = cable_d
gr_z       = Zb - wind_zback;// Nutmitte in Z (hinterste Windung)
wind_turns = 2;             // Anzahl gestapelter Nut-Windungen (1 Wdg ~41 cm;
                           //   2 => bis ~82 cm Kabel, reicht fuer 70 cm)
gr_pitch   = cable_d + 0.6;// Z-Abstand benachbarter Windungen (Nut-Mitten)
                           //   weitere Windungen liegen um je gr_pitch nach VORN
// Kabel-Durchfuehrung (-X): grosse Oeffnung, durch die der Micro-USB-Stecker
// einmal passt, dann schmaler Klemmschlitz, der das duenne Kabel haelt.
mub_w     = 11;             // * Micro-USB Stecker (Overmold) Breite  [BITTE MESSEN]
mub_h     = 7;             // * Micro-USB Stecker (Overmold) Hoehe   [BITTE MESSEN]
clamp_gap = cable_d - 0.4; // Klemmschlitz-Breite (< cable_d -> klemmt das Kabel)
mub_dz    = mub_h/2 + 1.5; // Abstand: Mitte grosse Oeffnung unter der Nut
mub_zc    = gr_z - mub_dz; // Z-Mitte der grossen Oeffnung

// ================================================================
//  Hilfen
// ================================================================
module rbox(sx, sy, sz, r) {          // in XY gerundeter Quader
    hull() for (x=[r, sx-r], y=[r, sy-r])
        translate([x, y, 0]) cylinder(r=r, h=sz);
}

module rrect2d(w, h, r) {             // 2D: gerundetes Rechteck (zentriert)
    hull() for (sx = [-1, 1], sy = [-1, 1])
        translate([sx*(w/2 - r), sy*(h/2 - r)]) circle(r = r);
}

module stadium2d(w, h) {              // 2D: Stadion (voll verrundete Enden), lange Achse = w
    r = h/2;
    hull() for (sx = [-1, 1]) translate([sx*(w/2 - r), 0]) circle(r = r);
}

// ================================================================
//  BODY  = Blende + Wanne
// ================================================================
module usbc_cut() {
    // 1) Stadion-Durchbruch (USB-C Kontur) komplett durch die +X Stirnwand.
    cz = chg_z0 + usbc_zc;            // Buchsen-Mitte in Z (ueber Platinen-Unterseite)
    translate([OX - wall - 1, OY/2, cz])
        rotate([0, 90, 0])
            linear_extrude(wall + 2)
                rotate([0, 0, 90])           // lange Achse -> world Y (Breite)
                    stadium2d(usbc_w, usbc_h);

    // 2) Option 1: Senkung von aussen (Restwand duenner -> Stecker-Koerper taucht ein)
    if (usbc_cb_d > 0)
        translate([OX - usbc_cb_d, OY/2, cz])
            rotate([0, 90, 0])
                linear_extrude(usbc_cb_d + 1)
                    rrect2d(usbc_cb_h, usbc_cb_w, min(usbc_cb_h, usbc_cb_w)/2 - 0.01);

    // 3) Option 2: Aussparung in der Innenwand fuer die Platinen-Vorderkante
    if (chg_xshift > 0)
        translate([OX - wall - 0.1, OY/2, chg_z0 + chg_t/2])
            rotate([0, 90, 0])
                linear_extrude(chg_xshift + 0.1)
                    rrect2d(chg_t + 2*fit, chg_w + 2*fit, 0.4);
}

module charger_holder() {
    // Offene Tasche: Platine faellt von hinten rein und liegt auf der Bodenplatte.
    // Kein Einclipsen (Halt spaeter ueber verloetete Kabel / bei Bedarf Nase nachruesten).
    sx   = chg_l + chg_fit + chg_extra;     // Board-Tiefe in X (+ Spiel + Reserve)
    x0   = OX - wall - sx;                  // -X Kante Board/Halter
    fw   = 1.8;                             // Wanddicke
    ft   = 1.2;                             // Bodenplatte-Dicke
    zbot = front + disp_d + 0.3;            // Unterkante Waende (knapp HINTER Matrix ~16.3)
    ztop = chg_z0 + chg_t + 2.2;            // Waende ~2,2 mm ueber Board -> Tasche zum Reinrutschen
    yL   = chg_y0 - chg_fit/2 - fw;         // -Y Wand Aussenkante (Spiel je Seite chg_fit/2)
    yR   = chg_y0 + chg_w + chg_fit/2;      // +Y Wand Innenkante -> Tasche MITTIG zum Loch (OY/2)
    // Bodenplatte (Oberkante = chg_z0 = Board-Unterseite), symmetrisch verteilt
    translate([x0, chg_y0 - chg_fit/2 - 0.3, chg_z0 - ft])
        cube([sx, chg_w + chg_fit + 0.6, ft]);
    // zwei Seitenwaende (Ueberlaenge in +X wird geclippt)
    for (yy = [yL, yR])
        translate([x0, yy, zbot]) cube([sx + wall, fw, ztop - zbot]);
    // Rueckwand (-X) verbindet die Waende -> steif
    translate([x0, yL, zbot]) cube([fw, (yR + fw) - yL, ztop - zbot]);
}

module matrix_guides() {
    // Zwei Rippen oben/unten, nur im Frontbereich (Z: front..disp_d) und nur
    // ueber dem Matrix-Slot (nicht in der LED-Bucht), zentrieren die 32-mm-
    // Matrix im hoeheren Innenraum.
    g = (inner_h - disp_h)/2 - mfit;    // Ueberstand nach innen (Matrix-Slot = disp_h+2*mfit)
    if (g > 0.3)
        for (yy = [wall, OY - wall - g])
            translate([matrix_x0, yy, front])
                cube([mat_slot_w, g, disp_d]);
}

module matrix_spacer() {
    // Abstandhalter zwischen LED-Bucht und Matrix: bildet den -X Endanschlag
    // der Matrix. Endet led_spacer_drop mm VOR der Matrix-Rueckseite (Z), damit
    // seitlich abstehende Matrix-Anschluss-Pins hinten am Rippen-Ende vorbeigehen;
    // der Anschlag an der Matrix-Vorderkante bleibt erhalten.
    h = max(1, disp_d - led_spacer_drop);
    if (led_en)
        translate([wall + led_bay, wall, front])
            cube([spacer, inner_h, h]);
}

module led_holes() {
    // Zwei Presssitz-Loecher durch die Front, vertikal gestapelt, in der
    // Bucht (X) zentriert, um OY/2 symmetrisch verteilt.
    cx = wall + led_bay/2;
    for (dy = [-1, 1])
        translate([cx, OY/2 + dy*led_pitch/2, -1])
            cylinder(d = led_d, h = front + 2);
}

module switch_slot() {
    // Schlitz fuer den Schieber durch -X-Wand + Kragen. Y = Knopf + Weg, Z = Knopf.
    sl = sw_act_l + sw_travel + 2*sw_fit;
    sw = sw_act_w + 2*sw_fit;
    translate([-shell - 1, OY/2 - sl/2, sw_zc - sw/2])
        cube([shell + wall + 2, sl, sw]);
}

module switch_flange_recess() {
    // Flache Tasche in der Innenseite der -X-Wand -> Flansch sitzt buendig und
    // gefuehrt. Tiefe = Flanschdicke + Spiel.
    d = sw_plate_t + 0.3;
    translate([wall - d, OY/2 - (sw_plate_l + 2*sw_fit)/2, sw_zc - (sw_plate_w + 2*sw_fit)/2])
        cube([d + 0.1, sw_plate_l + 2*sw_fit, sw_plate_w + 2*sw_fit]);
}

module switch_screws() {
    // 2 Kernloecher fuer M2 (selbstschneidend) vom Flansch (innen) in die Wand,
    // blind (enden vor der Kragen-Aussenflaeche).
    for (dy = [-1, 1])
        translate([wall + 0.1, OY/2 + dy*sw_hole_sp/2, sw_zc])
            rotate([0, -90, 0]) cylinder(d = sw_screw_d, h = wall + shell - 1);
}

module switch_dish() {
    // Flache, rundum VERRUNDETE Griffmulde im Kragen aussen (Huelle aus Eckkugeln
    // -> alle Kanten/Ecken mit Radius r). Der Schieber ragt hinein und ist gut mit
    // dem Finger erreichbar.
    sl = sw_act_l + sw_travel + 2*sw_fit;
    sw = sw_act_w + 2*sw_fit;
    r     = 2.5;               // Rundungsradius
    depth = 2.0;               // Mulden-Tiefe (in den Kragen)
    dy    = 3.0;               // Rand um den Schlitz in Y
    dz    = 1.6;               // Rand um den Schlitz in Z (schmaler -> unter dem Feed)
    bx = 0.1;                          // Kern-Box in X (Rest ist Rundung)
    by = (sl + 2*dy) - 2*r;            // Kern-Box in Y
    bz = (sw + 2*dz) - 2*r;            // Kern-Box in Z
    x0 = -shell + depth - (bx/2 + r);  // so, dass der Muldenboden bei -shell+depth liegt
    translate([x0, OY/2, sw_zc])
        hull() for (ix = [-1, 1], iy = [-1, 1], iz = [-1, 1])
            translate([ix*bx/2, iy*by/2, iz*bz/2]) sphere(r);
}

// Umlaufender Kragen (macht das Gehaeuse aussen groesser, glatte Flaechen).
module collar() {
    difference() {
        translate([-shell, -shell, 0])
            rbox(OX + 2*shell, OY + 2*shell, Zb, corner + shell);
        translate([-0.01, -0.01, -1])
            rbox(OX + 0.02, OY + 0.02, Zb + 2, corner);
    }
}

// Kragen am USB-C freistellen -> Port bleibt an der Original-Wand zugaenglich.
module collar_usbc_gap() {
    // Kragen um die USB-C-Buchse aussparen (Port bleibt an der Original-Wand
    // zugaenglich). RUNDUM VERRUNDET (Huelle aus 8 Eckkugeln r) -> weiche Mulde
    // statt scharfer Rechteck-Einkerbung (analog switch_dish).
    cz = chg_z0 + usbc_zc;
    r     = 2.5;                      // Rundungsradius
    wy    = usbc_cb_w + 8;            // Gesamt Y (wie bisher)
    wz    = usbc_cb_h + 6;            // Gesamt Z (wie bisher)
    x_in  = OX - 1;                   // Boden (an der Koerper-Wand)
    x_out = OX + shell + 1;           // ragt aus dem Kragen
    bx = (x_out - x_in) - 2*r;        // Kern-Box X (Rest ist Rundung)
    by = wy - 2*r;                    // Kern-Box Y
    bz = wz - 2*r;                    // Kern-Box Z
    translate([x_in + r + bx/2, OY/2, cz])
        hull() for (ix = [-1, 1], iy = [-1, 1], iz = [-1, 1])
            translate([ix*bx/2, iy*by/2, iz*bz/2]) sphere(r);
}

// Runde, eingelassene Nut: ein Kreis-Cutter (Radius gr_rc) wird entlang des
// Kragen-Umfangs gezogen (4 Geraden + 4 Viertel-Torus-Ecken). Der Kreismittel-
// punkt liegt gr_e unter der Aussenflaeche -> Tiefe = cable_d, Kanten verrundet.
// Ein umlaufender Kreis-Cutter (Radius rr) auf Hoehe gz, Mittellinie ee unter der
// Kragen-Aussenflaeche (4 Geraden + 4 Viertel-Torus-Ecken).
module groove_loop_at(gz, rr, ee) {
    xL = corner; xR = OX - corner; yB = corner; yT = OY - corner;   // Eck-Zentren
    xin = -shell + ee; xout = OX + shell - ee;                      // Cutter-Mittellinie
    yin = -shell + ee; yout = OY + shell - ee;
    Rcc = corner + shell - ee;                                      // Eck-Radius Mittellinie
    // Geraden (oben/unten entlang X, links/rechts entlang Y)
    for (yy = [yout, yin]) translate([xL, yy, gz]) rotate([0, 90, 0]) cylinder(r = rr, h = xR - xL);
    for (xx = [xout, xin]) translate([xx, yB, gz]) rotate([-90, 0, 0]) cylinder(r = rr, h = yT - yB);
    // Ecken (Viertel-Torus)
    translate([xR, yT, gz])                  rotate_extrude(angle = 90) translate([Rcc, 0]) circle(r = rr);
    translate([xL, yT, gz]) rotate([0,0, 90]) rotate_extrude(angle = 90) translate([Rcc, 0]) circle(r = rr);
    translate([xL, yB, gz]) rotate([0,0,180]) rotate_extrude(angle = 90) translate([Rcc, 0]) circle(r = rr);
    translate([xR, yB, gz]) rotate([0,0,270]) rotate_extrude(angle = 90) translate([Rcc, 0]) circle(r = rr);
}

// Haupt-Rundnut: Kreis (gr_rc), Mittellinie gr_e unter der Oberflaeche -> Tiefe cable_d.
module groove_loop(gz) { groove_loop_at(gz, gr_rc, gr_e); }

// Umlaufender Kanal mit FLACHEM Boden im Z-Band [z0,z1], Tiefe cable_d.
// = aeussere Kragenhaut (Dicke cable_d) im Z-Band herausgeschnitten. Fuellt den
// Bereich zwischen den End-Loops eben auf -> kein Grat/Spitze zwischen Windungen.
module groove_band(z0, z1) {
    r_in = max(0.4, corner + shell - cable_d);
    intersection() {
        difference() {
            translate([-shell, -shell, -1])
                rbox(OX + 2*shell, OY + 2*shell, Zb + 2, corner + shell);
            translate([-shell + cable_d, -shell + cable_d, -2])
                rbox(OX + 2*shell - 2*cable_d, OY + 2*shell - 2*cable_d, Zb + 4, r_in);
        }
        translate([-shell - 1, -shell - 1, z0])
            cube([OX + 2*shell + 2, OY + 2*shell + 2, z1 - z0]);
    }
}

// Aufwickel-Nut: gerundete End-Loops (hinten gr_z, vorn gr_z-(n-1)*gr_pitch) mit
// flachem Boden dazwischen -> breite Sammelnut fuer wind_turns Windungen, ohne
// Spitze. Bei wind_turns=1 bleibt es die einzelne Rundnut.
module groove_cut() {
    z_hi = gr_z;
    z_lo = gr_z - (wind_turns - 1) * gr_pitch;
    groove_loop(z_hi);
    groove_loop(z_lo);
    if (z_hi > z_lo) groove_band(z_lo, z_hi);
    // KONVEXER Roundover an beiden Aussenkanten der Nut: die Aussenflaeche rollt
    // gerundet in die Nut (kein zweiter Scoop). Cutter = Ecke MINUS Viertelkreis.
    op = sqrt(gr_rc*gr_rc - gr_e*gr_e);   // halbe Oeffnungsbreite der Rundnut
    rf = 1.2;                             // Roundover-Radius
    groove_rim_roundover(z_hi + op, rf, +1);
    groove_rim_roundover(z_lo - op, rf, -1);
}

// Umlaufender Rechteck-Cutter (Halbmasse hz in Z, hu radial), Mittellinie ee unter
// der Oberflaeche, auf Hoehe gz -> wie groove_loop_at, aber mit Box-Profil.
module groove_box_loop(gz, hz, hu, ee) {
    xL = corner; xR = OX - corner; yB = corner; yT = OY - corner;
    yout = OY + shell - ee; yin = -shell + ee; xout = OX + shell - ee; xin = -shell + ee;
    Rcc = corner + shell - ee;
    for (yy = [yout, yin]) translate([xL, yy - hu, gz - hz]) cube([xR - xL, 2*hu, 2*hz]);
    for (xx = [xout, xin]) translate([xx - hu, yB, gz - hz]) cube([2*hu, yT - yB, 2*hz]);
    for (cc = [[xR, yT, 0], [xL, yT, 90], [xL, yB, 180], [xR, yB, 270]])
        translate([cc[0], cc[1], gz]) rotate([0, 0, cc[2]])
            rotate_extrude(angle = 90) translate([Rcc - hu, -hz]) square([2*hu, 2*hz]);
}

// Konvexe Rand-Rundung an einer Nut-Aussenkante bei z_rim. dir=+1: Material auf
// der +z-Seite, dir=-1: auf der -z-Seite. Cutter = Box MINUS tangentialem Kreis
// -> nach dem Subtrahieren bleibt ein konvexer Viertelrund-Uebergang stehen.
module groove_rim_roundover(z_rim, rf, dir) {
    difference() {
        groove_box_loop(z_rim + dir*rf/2, rf/2, (rf + 0.5)/2, (rf - 0.5)/2);
        groove_loop_at(z_rim + dir*rf, rf, rf);
    }
}

// Kabel-Durchfuehrung (-X): Schluesselloch = grosse Oeffnung (Micro-USB-Stecker
// passt durch) + schmaler Klemmschlitz nach oben in die Nut (haelt das Kabel).
module cable_feed() {
    Lx = wall + shell + 3;
    // grosse Oeffnung fuer den Stecker
    translate([-shell - 1, OY/2, mub_zc]) rotate([0, 90, 0])
        linear_extrude(Lx) rrect2d(mub_h, mub_w, min(mub_w, mub_h)/2 - 0.2);
    // Verrundeter Einfuehrtrichter am aeusseren Mund -> weicher Rand statt Kante
    fd = 2.2;   // Trichter-Tiefe (in den Kragen)
    fl = 1.6;   // Aufweitung pro Seite am Mund
    hull() {
        translate([-shell - 0.01, OY/2, mub_zc]) rotate([0, 90, 0]) linear_extrude(0.01)
            rrect2d(mub_h + 2*fl, mub_w + 2*fl, (min(mub_w, mub_h) + 2*fl)/2 - 0.2);
        translate([-shell + fd, OY/2, mub_zc]) rotate([0, 90, 0]) linear_extrude(0.01)
            rrect2d(mub_h, mub_w, min(mub_w, mub_h)/2 - 0.2);
    }
    // Klemmschlitz hoch bis in die Nut
    translate([-shell - 1, OY/2, (mub_zc + gr_z)/2]) rotate([0, 90, 0])
        linear_extrude(Lx) rrect2d(gr_z - mub_zc, clamp_gap, clamp_gap/2 - 0.05);
}

// Verschluss-Stopfen fuer die grosse Oeffnung (nach dem Durchfuehren einsetzen).
// Fuellt die Oeffnung buendig; Aussenflansch als Anschlag/Griff; oben eine
// Freinut, damit das im Klemmschlitz sitzende Kabel nicht gequetscht wird.
module feed_plug() {
    cl  = 0.35;                              // Passungsspiel rundum
    Lp  = shell + 1.0;                        // Koerper gekuerzt: Kragen (shell=4.6) + 1 mm Wand
                                             //   = 5.6 mm statt voller Tiefe wall+shell=7.0.
                                             //   Fuellt den sichtbaren Kragen + Wandanfang; die
                                             //   inneren ~1.4 mm der Wand (Innenraum, verdeckt)
                                             //   bleiben frei -> weniger Material, nicht so klobig.
                                             //   Flansch bleibt Anschlag an der Kragen-Aussenflaeche.
    top = mub_zc + (mub_h - 2*cl)/2;         // Oberkante Stopfen
    difference() {
        union() {
            translate([-shell, OY/2, mub_zc]) rotate([0, 90, 0])
                linear_extrude(Lp)
                    rrect2d(mub_h - 2*cl, mub_w - 2*cl, (min(mub_w, mub_h) - 2*cl)/2 - 0.2);
            // Aussen-Flansch (Anschlag + Griff)
            translate([-shell - 1.4, OY/2, mub_zc]) rotate([0, 90, 0])
                linear_extrude(1.4) rrect2d(mub_h + 2.5, mub_w + 2.5, 1.5);
        }
        // Kabel-Freinut oben
        translate([-shell - 3, OY/2, top]) rotate([0, 90, 0])
            cylinder(d = cable_d + 0.4, h = Lp + 6);
    }
}

module body() {
    difference() {
        union() {
            // Aussenkoerper, vorne geschlossen, hinten offen
            rbox(OX, OY, Zb, corner);
            if (wind_en) collar();          // umlaufender Kragen (aussen)
        }
        // Innenraum (von hinten offen: +1 in Z)
        translate([wall, wall, front])
            rbox(inner_w, inner_h, inner_d + 1, max(0.4, corner - wall));
        // LED-Fenster durch die Front (mit dem Matrix-Slot ausgerichtet)
        translate([win_x0, (OY - win_h)/2, -1])
            cube([win_w, win_h, front + 2]);
        // Front-Loecher fuer die 2 Status-LEDs in der Bucht
        if (led_en) led_holes();
        // USB-C Ausschnitt
        usbc_cut();
        // Schalter SS-12F15: Schieber-Schlitz + Flansch-Tasche + M2-Kernloecher
        if (switch_enable) { switch_slot(); switch_flange_recess(); switch_screws(); }
        if (switch_enable && wind_en) switch_dish();   // Griffmulde im Kragen
        // Aufwickel: Kragen am USB-C oeffnen + Nut einlassen + Kabel-Durchfuehrung
        if (wind_en) { collar_usbc_gap(); groove_cut(); cable_feed(); }
    }
    // Einbauten sicher innerhalb der Wanne halten
    intersection() {
        union() {
            charger_holder();   // Lader-Halter an der Stirnwand
            matrix_guides();    // Zentrier-Rippen fuer die Matrix
            matrix_spacer();    // Abstandhalter Bucht <-> Matrix (-X Endanschlag)
        }
        translate([wall, wall, front])
            cube([inner_w, inner_h, inner_d]);
    }
}

// ================================================================
//  COVER = Rueckdeckel mit Reib-/Klipp-Lippe
// ================================================================
module cover() {
    lip_h = 4;                          // Tiefe der eingreifenden Lippe
    li_w  = inner_w - 2*covfit;
    li_h  = inner_h - 2*covfit;
    // Deckelplatte deckt die volle Aussenkontur ab (mit Kragen -> groesser)
    s   = wind_en ? shell : 0;
    union() {
        // Deckelplatte (Kragen-Aussenmass)
        translate([-s, -s, 0])
            rbox(OX + 2*s, OY + 2*s, covwall, corner + s);
        // Lippe, die in die Wanne greift
        translate([wall + covfit, wall + covfit, covwall - 0.01])
            difference() {
                rbox(li_w, li_h, lip_h, max(0.4, corner - wall));
                // aussparen -> Materialsparen + Snap-Elastizitaet
                translate([2, 2, -1])
                    rbox(li_w - 4, li_h - 4, lip_h + 2, 1);
            }
    }
}

// ================================================================
//  TEST-COUPON  = das -X-Endstueck (Kabel-Durchfuehrung, Kragen, runde Nut,
//  Deckel-Lippe) + Verschluss-Stopfen zum Passungs-Testdruck.
// ================================================================
module end_slab()  // Schneidkoerper: die ersten coupon_len in -X, inkl. Kragen
    translate([-shell - 1, -shell - 1, -1])
        cube([coupon_len + shell + 1, OY + 2*shell + 2, Zb + 2]);

module body_coupon()  intersection() { body();  end_slab(); }
module cover_coupon() intersection() { cover(); end_slab(); }

// +X-Endstueck (Ladeelektronik: USB-C, Lader-Tasche)
module end_slab_chg()  // die letzten coupon_len in +X, inkl. Kragen
    translate([OX - coupon_len, -shell - 1, -1])
        cube([coupon_len + shell + 1, OY + 2*shell + 2, Zb + 2]);

module body_coupon_chg()  intersection() { body();  end_slab_chg(); }
module cover_coupon_chg() intersection() { cover(); end_slab_chg(); }

// ================================================================
//  Ausgabe
// ================================================================
if (part == "body")  body();
if (part == "cover") cover();
if (part == "feed_plug") feed_plug();
if (part == "both") {
    body();
    translate([0, OY + 2*shell + 12, 0]) cover();
}
// Coupon -X (Kabel-Durchfuehrung)
if (part == "coupon_body")  body_coupon();
if (part == "coupon_cover") cover_coupon();
if (part == "testfit") {
    body_coupon();
    translate([0, OY + 2*shell + 12, 0]) cover_coupon();
    translate([coupon_len + 14, OY/2, 0]) rotate([0, -90, 0]) feed_plug();
}
// Coupon +X (Ladeelektronik: USB-C + Lader-Tasche)
if (part == "coupon_body_chg")  translate([-(OX - coupon_len), 0, 0]) body_coupon_chg();
if (part == "coupon_cover_chg") translate([-(OX - coupon_len), 0, 0]) cover_coupon_chg();
if (part == "testfit_chg") {
    translate([-(OX - coupon_len), 0, 0]) body_coupon_chg();
    translate([-(OX - coupon_len), OY + 2*shell + 12, 0]) cover_coupon_chg();
}
