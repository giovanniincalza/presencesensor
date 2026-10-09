#pragma once
#include <Arduino.h>
#include <vector>

// Elenco locale delle tessere autorizzate, salvato su LittleFS (/whitelist.csv)
// una riga per tessera: UID;nome;maschera_varchi
// maschera_varchi: bit 0 = varco 0 (pedonale), bit 1 = varco 1 (carrabile)

struct Entry {
  String uid;     // esadecimale maiuscolo senza separatori, es. "04A1B2C3"
  String name;
  uint8_t gates;
};

namespace Whitelist {
bool begin();
const std::vector<Entry> &all();
const Entry *find(const String &uid);
bool allowed(const String &uid, uint8_t gate);
bool add(const String &uid, const String &name, uint8_t gates);
bool remove(const String &uid);
bool normalizeUid(String &uid);  // maiuscolo, solo hex, lunghezza 8/14/20
String sanitizeName(const String &name);
}
