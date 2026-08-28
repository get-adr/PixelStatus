#pragma once
#include <Arduino.h>

// Schaetzt die CPU-Last aus der loop()-Frequenz. tick() muss in jedem Arduino-
// loop() laufen. Der ESP8266 hat keinen echten Last-Zaehler; als Naeherung gilt:
// je seltener loop() pro Sekunde durchlaeuft, desto mehr Zeit verbraucht die
// Arbeit -> hoehere Last. Bezugsgroesse ist die hoechste je gemessene Rate
// (praktisch der Idle-Durchsatz).
namespace LoopStats {
  void     tick();          // in jedem loop() aufrufen
  uint8_t  load();          // geschaetzte CPU-Last 0..100 %
  uint32_t loopsPerSec();   // zuletzt gemessene loop()-Durchlaeufe/Sekunde
}
