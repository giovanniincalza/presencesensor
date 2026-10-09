#include "Gate.h"

void Gate::relay(bool on) {
  digitalWrite(cfg.pinRelay, on == cfg.relayActiveHigh ? HIGH : LOW);
}

void Gate::begin() {
  pinMode(cfg.pinRelay, OUTPUT);
  relay(false);  // a riposo: serratura/cancello non comandati
  if (cfg.pinBlock >= 0) pinMode(cfg.pinBlock, INPUT_PULLUP);
  for (int8_t p : {cfg.pinLedOk, cfg.pinLedKo, cfg.pinBuzzer}) {
    if (p >= 0) {
      pinMode(p, OUTPUT);
      digitalWrite(p, LOW);
    }
  }
}

bool Gate::blocked() const {
  if (cfg.pinBlock < 0) return false;
  bool level = digitalRead(cfg.pinBlock);
  return cfg.blockActiveLow ? !level : level;
}

bool Gate::ready() const {
  return !pulsing && (!everOpened || millis() - lastOpen >= cfg.cooldownMs);
}

bool Gate::open() {
  if (blocked() || !ready()) return false;
  relay(true);
  pulsing = true;
  pulseStart = millis();
  lastOpen = pulseStart;
  everOpened = true;
  indicate(cfg.pinLedOk, cfg.pulseMs);
  if (cfg.pinBuzzer >= 0) {
    digitalWrite(cfg.pinBuzzer, HIGH);
    buzzUntil = millis() + 120;
  }
  return true;
}

void Gate::signalDenied() {
  indicate(cfg.pinLedKo, 800);
  if (cfg.pinBuzzer >= 0) {
    digitalWrite(cfg.pinBuzzer, HIGH);
    buzzUntil = millis() + 600;
  }
}

void Gate::indicate(int8_t ledPin, uint16_t ms) {
  if (ledPin < 0) return;
  if (activeLed >= 0 && activeLed != ledPin) digitalWrite(activeLed, LOW);
  activeLed = ledPin;
  digitalWrite(ledPin, HIGH);
  ledUntil = millis() + ms;
}

void Gate::update() {
  uint32_t now = millis();
  if (pulsing && now - pulseStart >= cfg.pulseMs) {
    relay(false);
    pulsing = false;
  }
  if (activeLed >= 0 && (int32_t)(now - ledUntil) >= 0) {
    digitalWrite(activeLed, LOW);
    activeLed = -1;
  }
  if (buzzUntil && (int32_t)(now - buzzUntil) >= 0) {
    digitalWrite(cfg.pinBuzzer, LOW);
    buzzUntil = 0;
  }
}
