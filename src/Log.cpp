#include "Log.h"
#include <LittleFS.h>
#include <time.h>
#include "config.h"

namespace {
const char *LOG_PATH = "/log.txt";
const char *LOG_OLD = "/log.old";

String timestamp() {
  time_t now = time(nullptr);
  if (now < 1700000000) {  // NTP non ancora sincronizzato
    return "T+" + String(millis() / 1000) + "s";
  }
  struct tm t;
  localtime_r(&now, &t);
  char buf[24];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
  return String(buf);
}
}  // namespace

namespace Log {

void event(const char *gate, const String &uid, const String &name, const char *outcome) {
  String line = timestamp() + ";" + gate + ";" + uid + ";" + name + ";" + outcome;
  Serial.println(line);

  File f = LittleFS.open(LOG_PATH, "r");
  size_t size = f ? f.size() : 0;
  if (f) f.close();
  if (size > LOG_MAX_BYTES) {
    LittleFS.remove(LOG_OLD);
    LittleFS.rename(LOG_PATH, LOG_OLD);
  }
  f = LittleFS.open(LOG_PATH, "a");
  if (f) {
    f.println(line);
    f.close();
  }
}

String tail(size_t maxBytes) {
  File f = LittleFS.open(LOG_PATH, "r");
  if (!f) return "";
  size_t size = f.size();
  if (size > maxBytes) {
    f.seek(size - maxBytes);
    f.readStringUntil('\n');  // scarta la riga troncata
  }
  String s = f.readString();
  f.close();
  return s;
}

}  // namespace Log
