#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------------------
// Configurazione. Modificare prima di caricare il firmware.
// ---------------------------------------------------------------------------

// Wi-Fi (rete locale per la pagina di gestione). Se la connessione fallisce
// il dispositivo apre un access point con le credenziali AP_*.
#define WIFI_SSID      "CAMBIAMI"
#define WIFI_PASS      "CAMBIAMI"
#define AP_SSID        "accessi-config"
#define AP_PASS        "cambiami123"     // minimo 8 caratteri

// Credenziali pagina web di gestione (HTTP basic auth).
#define ADMIN_USER     "admin"
#define ADMIN_PASS     "cambiami"

// NTP per data/ora nel log (se la rete ha accesso a internet o a un NTP interno).
#define NTP_SERVER     "pool.ntp.org"
#define TZ_INFO        "CET-1CEST,M3.5.0,M10.5.0/3"

// Bus SPI condiviso dai due PN532 (VSPI di default dell'ESP32).
#define PIN_SPI_SCK    18
#define PIN_SPI_MISO   19
#define PIN_SPI_MOSI   23

// ---------------------------------------------------------------------------
// Varchi: 0 = pedonale, 1 = carrabile
// ---------------------------------------------------------------------------
struct GateConfig {
  const char *name;
  uint8_t pinReaderSS;     // chip select del PN532
  uint8_t pinRelay;        // uscita verso relè (serratura / contatto "apri")
  bool    relayActiveHigh;
  uint16_t pulseMs;        // durata impulso di apertura
  uint16_t cooldownMs;     // tempo minimo tra due aperture
  int8_t  pinBlock;        // ingresso di sicurezza (fotocellula/spira), -1 = non usato
  bool    blockActiveLow;  // true: ingresso attivo a livello basso (contatto verso GND)
  int8_t  pinLedOk;        // LED verde, -1 = non usato
  int8_t  pinLedKo;        // LED rosso, -1 = non usato
  int8_t  pinBuzzer;       // buzzer, -1 = non usato
};

static const GateConfig GATES[] = {
  // nome        SS  relay  actH  pulse cool  block  blkLow ledOk ledKo buzz
  { "Pedonale",  5,  25,   true, 3000, 1000,  -1,   true,  26,   27,   13 },
  { "Carrabile", 4,  32,   true, 1000, 3000,  34,   true,  33,   14,   16 },
};
static const uint8_t GATE_COUNT = sizeof(GATES) / sizeof(GATES[0]);

// Il PN532 su SPI usa anche il pin RSTO/IRQ opzionali: non collegati.
#define LOG_MAX_BYTES  65536   // oltre questa soglia il log viene ruotato
