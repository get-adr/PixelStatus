#pragma once
#include "DisplayManager.h"

// Vordefinierte Status. Wird von Web-UI (/api/preset) und MQTT (.../cmd/preset)
// gemeinsam benutzt, damit beide Steuerwege dieselben Stati liefern.
// Neue Stati hier ergaenzen.
inline bool applyPreset(DisplayManager& d, const String& name) {
  String n = name;
  n.toLowerCase();
  if      (n == "onair" || n == "on_air") d.showMessage("ON AIR");
  else if (n == "call")                   d.showMessage("IN A CALL");
  else if (n == "busy")                   d.showMessage("BUSY");
  else if (n == "brb")                    d.showMessage("BRB");
  else if (n == "free")                   d.showMessage("FREE");
  else if (n == "off" || n == "clear")    d.clear();
  else return false;
  return true;
}
