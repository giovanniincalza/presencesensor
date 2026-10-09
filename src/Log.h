#pragma once
#include <Arduino.h>

namespace Log {
// Aggiunge una riga a /log.txt: data/ora;varco;UID;nome;esito
void event(const char *gate, const String &uid, const String &name, const char *outcome);
String tail(size_t maxBytes = 4096);
}
