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
    ina.configure(ADCRange::RANGE_163MV, ConvTime::CT_540US,
                  ConvTime::CT_540US, AVGCount::AVG_64);
    ina.setShuntCal(config.maxCurrentA, config.shuntOhm);
    Serial.println("[Sensors] INA228 OK");
  }

  // Stima SOC iniziale da tensione
  // Eseguito sempre all'avvio se la tensione è nel range plausibile per il profilo
  {
    delay(150); // attende prima conversione ADC valida
    float vNow = inaPresent ? ina.getBusVoltage() : 0.0f;
    const BatteryProfile& p = PROFILES[config.batteryType];
    float range = p.fullV - p.emptyV;
    // Accetta la lettura solo se la tensione è nel range plausibile (emptyV-1V ... fullV+1V)
    bool vPlausible = !isnan(vNow) && vNow >= (p.emptyV - 1.0f) && vNow <= (p.fullV + 1.0f);
    if (vPlausible && range > 0.1f) {
      float soc = constrain((vNow - p.emptyV) / range, 0.0f, 1.0f);
      ahUsed = batteryCapacityAh * (1.0f - soc);
      Serial.printf("[Sensors] SOC init da V=%.2f -> %.1f%%\n", vNow, soc * 100);
    } else {
      // Tensione fuori range (es. USB 5V) — usa valore NVS o default 50%
      if (config.ahUsedSaved > 0.0f && config.ahUsedSaved <= batteryCapacityAh) {
        ahUsed = config.ahUsedSaved;
        Serial.printf("[Sensors] SOC da NVS: ahUsed=%.1f\n", ahUsed);
      } else {
        ahUsed = batteryCapacityAh * 0.5f; // default 50% se tutto ignoto
        Serial.println("[Sensors] SOC default 50% (tensione non plausibile)");
      }
    }
  }

  pinMode(config.pinTankExcite, OUTPUT);
  digitalWrite(config.pinTankExcite, LOW);
  pinMode(config.pinTankBlack, INPUT);
  pinMode(config.pinTankGray1, INPUT);
  pinMode(config.pinTankGray2, INPUT);
  pinMode(config.pinTankGray3, INPUT);

  // Carica stato persistente
  voltageMin = config.voltageMinSeen;
  voltageMax = config.voltageMaxSeen;
  currentMin = config.currentMinSeen;
  currentMax = config.currentMaxSeen;
  ahUsed = config.ahUsedSaved;
  lastCoulombMs = millis();
  tankTimer = millis();
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
    currentA = isnan(iraw) ? 0 : iraw;
    powerW = voltageBus * currentA;
  } else {
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
  if (currentA < currentMin)   currentMin = currentA;
  if (currentA > currentMax)   currentMax = currentA;

  // Aggiorna buffer storico circolare
  history[histHead] = { voltageBus, currentA };
  histHead = (histHead + 1) % HIST_SIZE;
  if (histCount < HIST_SIZE) histCount++;
}

void SensorManager::updateCoulombCounter() {
  unsigned long now = millis();
  float dtH = (now - lastCoulombMs) / 3600000.0f;
  lastCoulombMs = now;
  if (simMode) dtH *= 10.0f;

  float eff = PROFILES[config.batteryType].coulombEff;
  float k   = PROFILES[config.batteryType].peukertExp;

  if (currentA > 0.05f) {
    // Carica: recupera capacità
    ahUsed -= currentA * eff * dtH;
  } else if (currentA < -0.05f) {
    // Scarica con correzione Peukert
    float iAbs = fabsf(currentA);
    float iRef = batteryCapacityAh / 20.0f;
    float iCorr = (k > 1.0f && iRef > 0)
                  ? powf(iAbs / iRef, k - 1.0f) * iAbs
                  : iAbs;
    ahUsed += iCorr * dtH;
  }

  ahUsed = constrain(ahUsed, 0.0f, batteryCapacityAh);

  // SOC da coulomb counter
  socAh = constrain(100.0f * (1.0f - ahUsed / batteryCapacityAh), 0.0f, 100.0f);
  if (isnan(socAh)) socAh = 50.0f;

  // SOC da tensione (in realtime, indipendente dal coulomb counter)
  const BatteryProfile& p = PROFILES[config.batteryType];
  float range = p.fullV - p.emptyV;
  if (range > 0.1f && voltageBus > 1.0f) {
    socV = constrain((voltageBus - p.emptyV) / range * 100.0f, 0.0f, 100.0f);
  } else {
    socV = socAh;
  }

  // SOC fuso: usa tensione se coulomb counter non è affidabile
  // (tensione fuori range plausibile = coulomb counter ha priorità)
  bool vInRange = (voltageBus >= (p.emptyV - 0.5f)) && (voltageBus <= (p.fullV + 0.5f));
  if (vInRange) {
    // Fusione: 70% coulomb + 30% tensione per stabilità
    batterySOC = 0.7f * socAh + 0.3f * socV;
  } else {
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

      // Resistenza sonda con pull-up 2.2MΩ, ADC 12-bit, 3.3V
      for (int i = 0; i < 4; i++) {
        if (tankADC[i] > 0 && tankADC[i] < 4095) {
          tankMegaOhm[i] = 2.2f * (4095.0f - tankADC[i]) / (float)tankADC[i];
        } else {
          tankMegaOhm[i] = (tankADC[i] >= 4095) ? 999.0f : 0.01f;
        }
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