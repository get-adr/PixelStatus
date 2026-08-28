#include "LoopStats.h"

namespace LoopStats {
  static uint32_t s_count = 0;    // Durchlaeufe in der laufenden Sekunde
  static uint32_t s_last  = 0;    // Zeitpunkt der letzten Auswertung
  static uint32_t s_lps   = 0;    // zuletzt gemessene Rate
  static uint32_t s_max   = 1;    // hoechste je gemessene Rate (Idle-Referenz)

  void tick() {
    s_count++;
    uint32_t now = millis();
    if (now - s_last >= 1000) {
      s_lps  = s_count;
      s_count = 0;
      s_last  = now;
      if (s_lps > s_max) s_max = s_lps;   // Idle-Spitze lernt sich mit der Zeit ein
    }
  }

  uint32_t loopsPerSec() { return s_lps; }

  uint8_t load() {
    if (s_max == 0 || s_lps >= s_max) return 0;
    return (uint8_t)((100UL * (s_max - s_lps)) / s_max);
  }
}
