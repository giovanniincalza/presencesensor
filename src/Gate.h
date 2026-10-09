#pragma once
#include <Arduino.h>
#include "config.h"

// Gestione di un varco: impulso relè non bloccante, cooldown, ingresso di
// sicurezza e segnalazioni acustiche/luminose.
class Gate {
 public:
  explicit Gate(uint8_t index) : idx(index), cfg(GATES[index]) {}

  void begin();
  void update();              // da chiamare spesso dal loop()

  bool blocked() const;       // ingresso di sicurezza attivo
  bool ready() const;         // cooldown trascorso
  bool open();                // false se bloccato o in cooldown
  void signalDenied();        // lampeggio rosso + beep
  const char *name() const { return cfg.name; }

 private:
  void relay(bool on);
  void indicate(int8_t ledPin, uint16_t ms);

  uint8_t idx;
  const GateConfig &cfg;
  bool pulsing = false;
  uint32_t pulseStart = 0;
  uint32_t lastOpen = 0;
  bool everOpened = false;
  int8_t activeLed = -1;
  uint32_t ledUntil = 0;
  uint32_t buzzUntil = 0;
};
