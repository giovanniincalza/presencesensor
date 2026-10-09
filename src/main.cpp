#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <Adafruit_PN532.h>
#include <time.h>

#include "config.h"
#include "Gate.h"
#include "Log.h"
#include "Whitelist.h"
#include "WebUI.h"

static Gate *gates[GATE_COUNT];
static Adafruit_PN532 *readers[GATE_COUNT];
static bool readerOk[GATE_COUNT];

// Antirimbalzo: stessa tessera sullo stesso lettore ignorata per 2 s.
static String lastUid[GATE_COUNT];
static uint32_t lastSeen[GATE_COUNT];
static const uint32_t REPEAT_MS = 2000;

static void startNetwork() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) delay(250);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("Wi-Fi OK, IP %s\n", WiFi.localIP().toString().c_str());
    configTzTime(TZ_INFO, NTP_SERVER);
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.printf("Wi-Fi non disponibile, AP '%s' su %s\n", AP_SSID,
                  WiFi.softAPIP().toString().c_str());
  }
}

static bool initReader(uint8_t i) {
  readers[i]->begin();
  uint32_t ver = readers[i]->getFirmwareVersion();
  if (!ver) return false;
  readers[i]->SAMConfig();
  return true;
}

static String uidToString(const uint8_t *uid, uint8_t len) {
  String s;
  for (uint8_t i = 0; i < len; i++) {
    if (uid[i] < 0x10) s += '0';
    s += String(uid[i], HEX);
  }
  s.toUpperCase();
  return s;
}

static void handleCard(uint8_t g, const String &uid) {
  const Entry *e = Whitelist::find(uid);
  String name = e ? e->name : "";

  if (!Whitelist::allowed(uid, g)) {
    gates[g]->signalDenied();
    WebUI::setLastUnknown(g, uid);
    Log::event(GATES[g].name, uid, name, e ? "NEGATO_VARCO" : "NEGATO_SCONOSCIUTA");
    return;
  }
  if (gates[g]->blocked()) {
    gates[g]->signalDenied();
    Log::event(GATES[g].name, uid, name, "BLOCCATO_SICUREZZA");
    return;
  }
  if (!gates[g]->open()) {
    return;  // in cooldown: nessun evento, evita righe duplicate
  }
  Log::event(GATES[g].name, uid, name, "APERTO");
}

static void pollReader(uint8_t g) {
  if (!readerOk[g]) return;
  uint8_t uid[7];
  uint8_t len = 0;
  // timeout breve: i due lettori vengono interrogati a turno
  if (!readers[g]->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &len, 40)) return;

  String s = uidToString(uid, len);
  uint32_t now = millis();
  if (s == lastUid[g] && now - lastSeen[g] < REPEAT_MS) {
    lastSeen[g] = now;
    return;
  }
  lastUid[g] = s;
  lastSeen[g] = now;
  handleCard(g, s);
}

void setup() {
  Serial.begin(115200);
  if (!LittleFS.begin(true)) Serial.println("LittleFS: errore");
  Whitelist::begin();

  // Relè a riposo il prima possibile.
  for (uint8_t i = 0; i < GATE_COUNT; i++) {
    gates[i] = new Gate(i);
    gates[i]->begin();
  }

  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);
  for (uint8_t i = 0; i < GATE_COUNT; i++) {
    readers[i] = new Adafruit_PN532(GATES[i].pinReaderSS, &SPI);
    readerOk[i] = initReader(i);
    Serial.printf("Lettore %s: %s\n", GATES[i].name, readerOk[i] ? "OK" : "NON TROVATO");
  }

  startNetwork();
  WebUI::begin();
  Serial.printf("Tessere in whitelist: %u\n", (unsigned)Whitelist::all().size());
}

void loop() {
  for (uint8_t i = 0; i < GATE_COUNT; i++) {
    gates[i]->update();
    pollReader(i);
  }
  WebUI::handle();

  uint8_t mg;
  if (WebUI::takeManualOpen(mg)) {
    bool ok = gates[mg]->open();
    Log::event(GATES[mg].name, "", "", ok ? "APERTO_MANUALE" : "MANUALE_RIFIUTATO");
  }

  // Se un lettore si è bloccato, riprova periodicamente a inizializzarlo.
  static uint32_t lastRetry = 0;
  if (millis() - lastRetry > 30000) {
    lastRetry = millis();
    for (uint8_t i = 0; i < GATE_COUNT; i++)
      if (!readerOk[i]) readerOk[i] = initReader(i);
  }
}
