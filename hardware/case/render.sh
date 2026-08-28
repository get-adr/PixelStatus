#!/usr/bin/env bash
# Rendert Vorschau-Ansichten des Gehaeuses nach ./renders/
# Aufruf:  ./render.sh      (aus hardware/case/)
set -e
cd "$(dirname "$0")"
mkdir -p renders
SZ=1400,950
CAM="--viewall --autocenter"
COL="--colorscheme=PixelStatus Black"   # schwarzes Material, hellgrauer Hintergrund
                                         # (siehe ~/Library/Application Support/OpenSCAD/color-schemes/render/)

# Gesamtansicht (Body + Deckel)
openscad "$COL" -o renders/01_iso.png       --imgsize=$SZ $CAM --camera=0,0,0,60,0,20,0  -D 'part="both"' case.scad
# Body von vorn (Blende + LED-Fenster + LED-Bucht) -- frontaler Blick auf die Z=0-Flaeche
openscad "$COL" -o renders/02_front.png     --imgsize=1400,560 --viewall --projection=ortho --camera=79,19,-260,79,19,0 -D 'part="body"' case.scad
# Nahaufnahme der LED-Bucht (-X): 2 Front-Loecher + Abstandhalter (Front-Ansicht des -X-Coupons)
openscad "$COL" -o renders/10_led_bay.png   --imgsize=900,700 --viewall --projection=ortho --camera=16,19,-260,16,19,0 -D 'part="coupon_body"' case.scad
# Schalter SS-12F15 (-X): Schieber-Schlitz + Kragen-Griffmulde (Aussen-Iso des -X-Coupons)
openscad "$COL" -o renders/11_switch.png    --imgsize=1000,850 --viewall --camera=0,0,0,72,0,235,0 -D 'part="coupon_body"' case.scad
# Body von hinten/innen (Lader-Halter, USB-C- und Schalter-Ausschnitt)
openscad "$COL" -o renders/03_innen.png     --imgsize=$SZ $CAM --camera=0,0,0,60,0,135,0 -D 'part="body"' case.scad

# Halbschnitt (Y-Haelfte entfernt) -> zeigt Lader-Halter HINTER der Matrix
cat > _section.scad <<EOF
part="none";
include <case.scad>
difference(){ body(); translate([-1, OY/2, -1]) cube([OX+2, OY+2, Zb+2]); }
EOF
openscad "$COL" -o renders/04_schnitt.png   --imgsize=$SZ $CAM --camera=0,0,0,70,0,20,0 _section.scad
rm -f _section.scad

# Nahaufnahme der USB-C Oeffnung (Blick von aussen auf die +X-Stirnwand)
# Ziel-Z = chg_z0 + usbc_zc = (front+disp_d+chg_gap) + usbc_zc ; bei Aenderung anpassen
openscad "$COL" -o renders/05_usbc.png      --imgsize=900,900 --camera=192,19.3,19.7,131,19.3,19.7 -D 'part="body"' case.scad
# Test-Coupon (Body-Endstueck + Deckel-Endstueck)
openscad "$COL" -o renders/06_coupon.png    --imgsize=$SZ $CAM --camera=0,0,0,55,0,205,0 -D 'part="testfit"' case.scad
# Aufwickel-Nut (Body + Deckel, hinten-iso)
openscad "$COL" -o renders/07_winder.png    --imgsize=$SZ $CAM --camera=0,0,0,58,0,25,0  -D 'part="both"' case.scad
# Kabel-Durchfuehrung + Verschluss-Stopfen
openscad "$COL" -o renders/08_feed_plug.png --imgsize=$SZ $CAM --camera=0,0,0,62,0,20,0  -D 'part="feed_plug"' case.scad
# Coupon +X (Ladeelektronik: USB-C + Lader-Tasche)
openscad "$COL" -o renders/09_coupon_chg.png --imgsize=$SZ $CAM --camera=0,0,0,60,0,135,0 -D 'part="coupon_body_chg"' case.scad

echo "Fertig -> $(pwd)/renders/"
ls -1 renders/
