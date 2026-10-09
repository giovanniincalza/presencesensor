#include "Whitelist.h"
#include <LittleFS.h>

namespace {
const char *FILE_PATH = "/whitelist.csv";
std::vector<Entry> entries;

bool save() {
  File f = LittleFS.open(FILE_PATH, "w");
  if (!f) return false;
  for (const Entry &e : entries) {
    f.printf("%s;%s;%u\n", e.uid.c_str(), e.name.c_str(), e.gates);
  }
  f.close();
  return true;
}
}  // namespace

namespace Whitelist {

bool normalizeUid(String &uid) {
  uid.trim();
  uid.toUpperCase();
  String out;
  for (size_t i = 0; i < uid.length(); i++) {
    char c = uid[i];
    if (isxdigit(c)) out += c;
    else if (c != ' ' && c != ':' && c != '-') return false;
  }
  uid = out;
  return uid.length() == 8 || uid.length() == 14 || uid.length() == 20;
}

String sanitizeName(const String &name) {
  String out;
  for (size_t i = 0; i < name.length() && out.length() < 32; i++) {
    char c = name[i];
    if (c < 32 || c == ';' || c == '"' || c == '\\' || c == '<' || c == '>') continue;
    out += c;
  }
  out.trim();
  return out;
}

bool begin() {
  entries.clear();
  if (!LittleFS.exists(FILE_PATH)) return true;
  File f = LittleFS.open(FILE_PATH, "r");
  if (!f) return false;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    int a = line.indexOf(';');
    int b = line.lastIndexOf(';');
    if (a < 1 || b <= a) continue;
    Entry e;
    e.uid = line.substring(0, a);
    e.name = line.substring(a + 1, b);
    e.gates = (uint8_t)line.substring(b + 1).toInt();
    if (normalizeUid(e.uid)) entries.push_back(e);
  }
  f.close();
  return true;
}

const std::vector<Entry> &all() { return entries; }

const Entry *find(const String &uid) {
  for (const Entry &e : entries)
    if (e.uid == uid) return &e;
  return nullptr;
}

bool allowed(const String &uid, uint8_t gate) {
  const Entry *e = find(uid);
  return e && (e->gates & (1 << gate));
}

bool add(const String &uid, const String &name, uint8_t gates) {
  for (Entry &e : entries) {
    if (e.uid == uid) {  // aggiorna
      e.name = name;
      e.gates = gates;
      return save();
    }
  }
  entries.push_back({uid, name, gates});
  return save();
}

bool remove(const String &uid) {
  for (size_t i = 0; i < entries.size(); i++) {
    if (entries[i].uid == uid) {
      entries.erase(entries.begin() + i);
      return save();
    }
  }
  return false;
}

}  // namespace Whitelist
