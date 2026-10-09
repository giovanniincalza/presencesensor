#pragma once
#include <Arduino.h>

namespace WebUI {
void begin();
void handle();
// Ultima tessera sconosciuta letta (per l'enrollment dalla pagina web).
void setLastUnknown(uint8_t gate, const String &uid);
// Richiesta di apertura manuale dalla pagina web (gestita nel loop principale).
bool takeManualOpen(uint8_t &gate);
}
