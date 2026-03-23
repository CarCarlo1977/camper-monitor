// ============================================================
//  config.h - Configurazione persistente ESP32
// ============================================================
#pragma once
#include <Arduino.h>
#include <Preferences.h>

#define BAT_TYPE_COUNT 5

struct BatteryProfile {
  const char* name;
  float coulombEff;
  float peukertExp;
  float nominalV;      // tensione nominale (V)
  float fullV;         // tensione a piena carica (V)
  float emptyV;        // tensione a scarica (V)
};

// nominalV / fullV / emptyV per stima SOC da tensione
static const BatteryProfile PROFILES[BAT_TYPE_COUNT] = {
  {"AGM",     0.85f, 1.25f, 12.0f, 12.7f, 11.6f},
  {"GEL",     0.85f, 1.20f, 12.0f, 12.7f, 11.6f},
  {"Pb-open", 0.80f, 1.30f, 12.0f, 12.7f, 11.4f},
  {"LiFePO4", 0.99f, 1.00f, 12.8f, 14.4f, 12.0f},
  {"Li-Ion",  0.98f, 1.00f, 11.1f, 12.6f, 10.5f}
};

class Config {
public:
  // Batteria
  float vHighThreshold    = 14.8f;
  float vLowThreshold     = 11.5f;
  float iHighThreshold    = 100.0f;
  float batteryCapacityAh = 100.0f;
  uint8_t batteryType     = 0;
  float shuntOhm          = 0.00025f;
  float maxCurrentA        = 200.0f;
  float nominalVoltage     = 12.0f;  // tensione nominale configurabile
  float currentScale       = 4.85f;  // fattore scala corrente (calibrazione)

  // Serbatoi
  uint16_t tankBlackThreshold = 750;
  uint16_t tankGrayThreshold  = 940;
  bool enableTankMonitoring   = true;

  // GPIO
  int pinI2C_SDA    = 21;
  int pinI2C_SCL    = 22;
  int pinTankBlack  = 35;
  int pinTankGray1  = 34;
  int pinTankGray2  = 39;
  int pinTankGray3  = 36;
  int pinTankExcite = 14;

  // WiFi
  bool wifiAPMode       = true;
  String wifiSSID       = "Camper-Monitor";
  String wifiPassword   = "12345678";
  String wifiSTA_SSID   = "";
  String wifiSTA_Password = "";

  // Sistema
  bool debugSerial = true;

  // Persistenti
  float voltageMinSeen =  100.0f;
  float voltageMaxSeen =    0.0f;
  float currentMinSeen = 1000.0f;
  float currentMaxSeen =    0.0f;
  float ahUsedSaved    =    0.0f;

  void load();
  void save();
  void saveMinMax(float vmin, float vmax, float imin, float imax);
  void saveAhUsed(float ah);

private:
  Preferences prefs;
};

extern Config config;