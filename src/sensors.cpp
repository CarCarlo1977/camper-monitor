// ============================================================
//  sensors.cpp - Implementazione SensorManager
// ============================================================
#include "sensors.h"

SensorManager sensors;

void SensorManager::init(int sda, int scl) {
  Wire.begin(sda, scl);

  if (!ina.begin(0x40, &Wire)) {
    inaPresent = false; simMode = true;
    Serial.println("[Sensors] INA228 non trovato - simulazione");
  } else {
    inaPresent = true; simMode = false;
    // RANGE_163MV: range standard, formula CAL semplice e verificata
    // AVG_256 + CT_1052US: filtra rumore EMI sui cavi di sense
    ina.configure(ADCRange::RANGE_163MV, ConvTime::CT_1052US,
                  ConvTime::CT_1052US, AVGCount::AVG_256);
    ina.setShuntCal(config.maxCurrentA, config.shuntOhm);
    Serial.println("[Sensors] INA228 OK");
  }
Serial.printf("[DEBUG] Appena letto da config: %.2f Ah\n", config.ahUsedSaved);
  // Carica lo stato persistente Ah
  ahUsed = config.ahUsedSaved; 
  batteryCapacityAh = config.batteryCapacityAh;
  
  // Inizializza il timer per evitare salti nel coulomb counter
  lastCoulombMs = millis();
  
  Serial.printf("[Sensors] Stato iniziale Ah caricato: %.2f\n", ahUsed);

  pinMode(config.pinTankExcite, OUTPUT);
  digitalWrite(config.pinTankExcite, LOW);
  pinMode(config.pinTankBlack, INPUT);
  pinMode(config.pinTankGray1, INPUT);
  pinMode(config.pinTankGray2, INPUT);
  pinMode(config.pinTankGray3, INPUT);

  // Carica stato persistente min/max
  voltageMin = config.voltageMinSeen;
  voltageMax = config.voltageMaxSeen;
  currentMin = config.currentMinSeen;
  currentMax = config.currentMaxSeen;

  // Stima SOC iniziale da OCV (dopo configure per lettura valida)
  // --- NUOVA LOGICA DI INIZIALIZZAZIONE SOC (Sostituisci da riga 45 circa) ---
  if (inaPresent) {
    delay(600); // Aspetta che l'INA faccia la prima conversione completa (AVG_256)
    
    // 1. Priorità assoluta al valore salvato in NVS (se sensato)
    if (config.ahUsedSaved >= 0.0f && config.ahUsedSaved <= batteryCapacityAh) {
      ahUsed = config.ahUsedSaved;
      Serial.printf("[Sensors] SOC caricato da NVS: ahUsed=%.2f\n", ahUsed);
    } 
    // 2. Fallback: Se NVS è vuoto o fuori range, stima dalla tensione
    else {
      float vNow = ina.getBusVoltage();
      const BatteryProfile& p = PROFILES[config.batteryType];
      float vRange = p.fullV - p.emptyV;
      if (vNow > 5.0f && vRange > 0.1f) {
        float soc = constrain((vNow - p.emptyV) / vRange, 0.0f, 1.0f);
        ahUsed = batteryCapacityAh * (1.0f - soc);
        Serial.printf("[Sensors] NVS vuoto, stima SOC da V(%.2fV): %.1f%%\n", vNow, soc * 100.0f);
      } else {
        ahUsed = batteryCapacityAh * 0.5f; // Ultima spiaggia: 50%
        Serial.println("[Sensors] SOC default 50% (nessun dato disponibile)");
      }
    }
  } else {
    ahUsed = config.ahUsedSaved;
  }

  lastCoulombMs = millis();
  tankTimer     = millis();
}

void SensorManager::reInitINA() {
  if (!inaPresent) return;
  ina.setShuntCal(config.maxCurrentA, config.shuntOhm);
}

void SensorManager::readBattery() {
  if (inaPresent && !simMode) {
    float vraw = ina.getBusVoltage();
    voltageBus = (isnan(vraw) || vraw < 0) ? 0 : vraw;

    float iraw = ina.getCurrent();
    if (isnan(iraw)) {
      currentA = 0;
    } else {
      // Scala corrente: corregge errore sistematico di calibrazione shunt
      float scaled = iraw * config.currentScale;
      // Dead-band 0.02A dopo scala
      currentA = (fabsf(scaled) < 0.02f) ? 0.0f : scaled;
    }
    powerW = voltageBus * currentA;
  } else {
    // Simulazione
    static unsigned long simStart = 0;
    static uint8_t simPhase = 0;
    if (simStart == 0) simStart = millis();
    if (millis() - simStart >= 3000UL) {
      simPhase = (simPhase + 1) % 3;
      simStart = millis();
    }
    float soc = constrain(100.0f * (1.0f - ahUsed / batteryCapacityAh), 0, 100);
    voltageBus = 11.0f + (soc / 100.0f) * 3.0f;
    switch (simPhase) {
      case 0: currentA = -15.0f; break;
      case 1: currentA =   8.0f; break;
      case 2: currentA =   0.0f; break;
    }
    powerW = voltageBus * currentA;
  }

  if (voltageBus < voltageMin) voltageMin = voltageBus;
  if (voltageBus > voltageMax) voltageMax = voltageBus;
  if (currentA   < currentMin) currentMin = currentA;
  if (currentA   > currentMax) currentMax = currentA;

  // Buffer storico circolare per grafico
  history[histHead] = { voltageBus, currentA };
  histHead  = (histHead + 1) % HIST_SIZE;
  if (histCount < HIST_SIZE) histCount++;
}

void SensorManager::updateCoulombCounter() {
  unsigned long now = millis();
  float dtH = (now - lastCoulombMs) / 3600000.0f;
  lastCoulombMs = now;
  if (simMode) dtH *= 10.0f;

  const BatteryProfile& p = PROFILES[config.batteryType];
  float eff = p.coulombEff;
  float k   = p.peukertExp;

  // === Coulomb counter con correzione Peukert ===
  if (currentA > 0.05f) {
    // Carica: recupera capacità
    ahUsed -= currentA * eff * dtH;
  } else if (currentA < -0.05f) {
    // Scarica con correzione Peukert
    float iAbs = fabsf(currentA);
    float iRef  = batteryCapacityAh / 20.0f;
    float iCorr = (k > 1.0f && iRef > 0)
                  ? powf(iAbs / iRef, k - 1.0f) * iAbs
                  : iAbs;
    ahUsed += iCorr * dtH;
  }
  ahUsed = constrain(ahUsed, 0.0f, batteryCapacityAh);

  // SOC da coulomb counter
  socAh = constrain(100.0f * (1.0f - ahUsed / batteryCapacityAh), 0.0f, 100.0f);
  if (isnan(socAh)) socAh = 50.0f;

  // === SOC da tensione con compensazione voltage sag (OCV stimato) ===
  // Vmis = OCV - I*Ri  →  OCV = Vmis + I*Ri
  // currentA > 0 (carica): tensione sovrastimata → OCV = Vmis - I*Ri
  // currentA < 0 (scarica): tensione sottostimata → OCV = Vmis + |I|*Ri
  // In entrambi i casi: OCV = Vmis - currentA * Ri  (segno già incluso)
  float vRange = p.fullV - p.emptyV;
  if (vRange > 0.1f && voltageBus > 1.0f) {
    // Ri scala inversamente con la capacità
    float Ri = p.internalR * (100.0f / batteryCapacityAh);
    float ocv = voltageBus - (currentA * Ri);
    ocv = constrain(ocv, p.emptyV - 1.0f, p.fullV + 1.0f);
    socV = constrain((ocv - p.emptyV) / vRange * 100.0f, 0.0f, 100.0f);
  } else {
    socV = socAh;
  }

  // === Fusione adattiva SOC ===
  // A riposo (|I| < 0.5A): OCV affidabile → peso maggiore a socV
  // Sotto carico leggero: mix bilanciato
  // Sotto carico pesante: coulomb counter affidabile → peso quasi tutto su socAh
  bool vInRange = (voltageBus >= (p.emptyV - 0.5f)) && (voltageBus <= (p.fullV + 0.5f));
  if (vInRange) {
    float iAbs = fabsf(currentA);
    float wV = (iAbs < 0.5f)  ? 0.6f :   // riposo: 60% tensione OCV
               (iAbs < 5.0f)  ? 0.2f :   // carico leggero: 20%
               (iAbs < 20.0f) ? 0.1f :   // carico medio: 10%
                                 0.05f;   // carico pesante: 5%
    batterySOC = (1.0f - wV) * socAh + wV * socV;
  } else {
    // Tensione fuori range plausibile (USB, disconnesso): solo coulomb counter
    batterySOC = socAh;
  }
  batterySOC = constrain(batterySOC, 0.0f, 100.0f);
}

void SensorManager::updateTanksFSM() {
  if (!config.enableTankMonitoring) return;
  unsigned long now = millis();

  switch (tankState) {
    case TK_IDLE:
      if (now - tankTimer >= TANK_CYCLE_MS) {
        digitalWrite(config.pinTankExcite, HIGH);
        tankTimer = now;
        tankState = TK_EXCITING;
      }
      break;
    case TK_EXCITING:
      if (now - tankTimer >= TANK_SETTLE_MS) {
        tankTimer = now;
        tankState = TK_READING;
      }
      break;
    case TK_READING: {
      uint32_t raw[4] = {0,0,0,0};
      for (int s = 0; s < 4; s++) {
        raw[0] += analogRead(config.pinTankBlack);
        raw[1] += analogRead(config.pinTankGray1);
        raw[2] += analogRead(config.pinTankGray2);
        raw[3] += analogRead(config.pinTankGray3);
        delayMicroseconds(200);
      }
      digitalWrite(config.pinTankExcite, LOW);

      for (int i = 0; i < 4; i++)
        tankADC[i] = (uint16_t)(raw[i] / 4);

      for (int i = 0; i < 4; i++) {
        if (tankADC[i] > 0 && tankADC[i] < 4095)
          tankMegaOhm[i] = 2.2f * (4095.0f - tankADC[i]) / (float)tankADC[i];
        else
          tankMegaOhm[i] = (tankADC[i] >= 4095) ? 999.0f : 0.01f;
      }

      tankBlack = (tankADC[0] < config.tankBlackThreshold) ? 1 : 0;

      uint8_t lvl = 0;
      if (tankADC[1] < config.tankGrayThreshold) lvl++;
      if (tankADC[2] < config.tankGrayThreshold) lvl++;
      if (tankADC[3] < config.tankGrayThreshold) lvl++;
      tankGray = lvl;

      tankTimer = now;
      tankState = TK_IDLE;
      break;
    }
  }
}

void SensorManager::resetMinMax() {
  voltageMin = 100; voltageMax = 0;
  currentMin = 1000; currentMax = 0;
}