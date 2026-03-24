// ============================================================
//  sensors.h - Lettura sensori
// ============================================================
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "INA228.h"
#include "config.h"

class SensorManager {
public:
  float voltageBus = 0, currentA = 0, powerW = 0;
  float voltageMin = 100, voltageMax = 0;
  float currentMin = 1000, currentMax = 0;

  uint8_t  tankGray = 0, tankBlack = 0;
  uint16_t tankADC[4] = {4095,4095,4095,4095};
  float    tankMegaOhm[4] = {0,0,0,0};

  float batteryCapacityAh = 100;
  float batterySOC = 100, ahUsed = 0;
  float socV  = 100;   // SOC da tensione (realtime)
  float socAh = 100;   // SOC da coulomb counter
  unsigned long lastCoulombMs = 0;

  bool inaPresent = false, simMode = false;

  // Storico circolare per grafico
  static const int HIST_SIZE = 120;
  struct HistPoint { float v; float i; };
  HistPoint history[HIST_SIZE];
  int histHead  = 0;
  int histCount = 0;

  void init(int sda, int scl);
  void readBattery();
  void updateTanksFSM();
  void updateCoulombCounter();
  void resetMinMax();
  void reInitINA();   // dopo cambio shunt da config
  void debugGrayTanks();

private:
  INA228Driver ina;
  enum TankFSM { TK_IDLE, TK_EXCITING, TK_READING };
  TankFSM tankState = TK_IDLE;
  unsigned long tankTimer = 0;
  static const unsigned long TANK_CYCLE_MS  = 15000UL;  // 10 secondi
  static const unsigned long TANK_SETTLE_MS = 70UL;      // 70ms
};

extern SensorManager sensors;